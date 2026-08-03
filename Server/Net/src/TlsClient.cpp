#include "TlsClient.h"

#include <spdlog/spdlog.h>
#include <ws2tcpip.h>

#include <stdexcept>

namespace heaven::net {

namespace {

constexpr ULONG_PTR kIoKey = 1;

std::string lastSocketError(const char* what) {
    return std::string(what) + " failed: WSA error " + std::to_string(WSAGetLastError());
}

struct WinsockScope {
    WinsockScope() {
        WSADATA data;
        if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }
    ~WinsockScope() { ::WSACleanup(); }
};

}  // namespace

TlsClient::TlsClient() : context_(), tls_(context_.native(), TlsRole::Client) {
    static WinsockScope winsock;  // 프로세스당 한 번
}

TlsClient::~TlsClient() {
    close();
    wait();
    if (iocp_ != nullptr) {
        ::CloseHandle(iocp_);
    }
    if (socket_ != INVALID_SOCKET) {
        ::closesocket(socket_);
    }
}

void TlsClient::connect(const std::string& host, std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* resolved = nullptr;
    const std::string service = std::to_string(port);
    if (::getaddrinfo(host.c_str(), service.c_str(), &hints, &resolved) != 0) {
        throw std::runtime_error("cannot resolve " + host + ":" + service);
    }

    socket_ = ::WSASocketW(resolved->ai_family, resolved->ai_socktype, resolved->ai_protocol,
                           nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (socket_ == INVALID_SOCKET) {
        ::freeaddrinfo(resolved);
        throw std::runtime_error(lastSocketError("WSASocketW"));
    }

    const int rc = ::connect(socket_, resolved->ai_addr, static_cast<int>(resolved->ai_addrlen));
    ::freeaddrinfo(resolved);
    if (rc == SOCKET_ERROR) {
        throw std::runtime_error("cannot connect to " + host + ":" + service + " (WSA error " +
                                 std::to_string(WSAGetLastError()) + ")");
    }

    iocp_ = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket_), nullptr, kIoKey, 0);
    if (iocp_ == nullptr) {
        throw std::runtime_error("CreateIoCompletionPort failed");
    }

    connected_.store(true, std::memory_order_release);
    worker_ = std::thread([this] { workerLoop(); });

    // 클라이언트가 ClientHello 를 먼저 보내야 한다.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tls_.startHandshake();
        flushLocked();
    }
    postRecv();

    // 핸드셰이크가 끝날 때까지 기다린다.
    std::unique_lock<std::mutex> lock(handshakeMutex_);
    handshakeCv_.wait(lock, [this] { return handshakeDone_ || handshakeFailed_; });
    if (handshakeFailed_) {
        throw std::runtime_error("TLS handshake failed: " + closeReason());
    }
}

bool TlsClient::postRecv() {
    if (closed_.load(std::memory_order_acquire)) {
        return false;
    }

    ZeroMemory(static_cast<OVERLAPPED*>(&recvOp_), sizeof(OVERLAPPED));
    WSABUF buffer;
    buffer.buf = recvBuffer_;
    buffer.len = static_cast<ULONG>(sizeof(recvBuffer_));

    DWORD flags = 0;
    pendingOps_.fetch_add(1, std::memory_order_acq_rel);
    const int rc = WSARecv(socket_, &buffer, 1, nullptr, &flags, &recvOp_, nullptr);
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        noteReason(lastSocketError("WSARecv"));
        pendingOps_.fetch_sub(1, std::memory_order_acq_rel);
        close();
        return false;
    }
    return true;
}

void TlsClient::workerLoop() {
    for (;;) {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* overlapped = nullptr;

        const BOOL ok = ::GetQueuedCompletionStatus(iocp_, &bytes, &key, &overlapped, INFINITE);
        if (overlapped == nullptr) {
            // close() 가 보낸 깨우기 신호. 취소된 작업이 아직 남아 있다면 그것들이
            // 완료되는 것까지 보고 나가야 버퍼를 안전하게 놓을 수 있다.
            if (pendingOps_.load(std::memory_order_acquire) == 0) {
                break;
            }
            continue;
        }

        auto* operation = static_cast<IoOperation*>(overlapped);
        if (operation->type == OpType::Recv) {
            onRecvComplete(bytes, ok == TRUE);
        } else {
            onSendComplete(bytes, ok == TRUE);
        }

        if (closed_.load(std::memory_order_acquire) &&
            pendingOps_.load(std::memory_order_acquire) == 0) {
            break;
        }
    }
    finish();
}

void TlsClient::onRecvComplete(DWORD bytes, bool ok) {
    pendingOps_.fetch_sub(1, std::memory_order_acq_rel);

    if (!ok || bytes == 0) {
        noteReason(bytes == 0 ? "server closed the connection"
                              : "receive failed (WSA error " +
                                    std::to_string(WSAGetLastError()) + ")");
        close();
        return;
    }

    std::vector<proto::Bytes> frames;
    bool fatal = false;
    bool justFinishedHandshake = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool wasHandshaking = !tls_.handshakeDone();

        tls_.feedEncrypted(recvBuffer_, bytes);
        if (!tls_.pump(plainAccum_)) {
            noteReason("TLS: " + tls_.lastError());
            fatal = true;
        } else {
            justFinishedHandshake = wasHandshaking && tls_.handshakeDone();

            // 완성된 프레임을 뽑는다.
            for (;;) {
                if (plainAccum_.size() < proto::kHeaderBytes) break;
                const std::uint32_t length = proto::decodeLength(plainAccum_.data());
                if (length == 0 || length > proto::kMaxBodyBytes) {
                    noteReason("protocol: frame length out of range (" +
                               std::to_string(length) + ")");
                    fatal = true;
                    break;
                }
                if (plainAccum_.size() < proto::kHeaderBytes + length) break;

                const auto begin =
                    plainAccum_.begin() + static_cast<std::ptrdiff_t>(proto::kHeaderBytes);
                frames.emplace_back(begin, begin + static_cast<std::ptrdiff_t>(length));
                plainAccum_.erase(plainAccum_.begin(),
                                  begin + static_cast<std::ptrdiff_t>(length));
            }
        }
        flushLocked();
    }

    if (justFinishedHandshake) {
        std::lock_guard<std::mutex> lock(handshakeMutex_);
        handshakeDone_ = true;
        handshakeCv_.notify_all();
    }

    if (fatal) {
        close();
        return;
    }

    if (onFrame_) {
        for (const proto::Bytes& body : frames) {
            onFrame_(body);
        }
    }

    // 일시정지 상태면 재게시하지 않는다. 수신 버퍼가 차서 상대의 전송이 막힌다.
    if (!paused_.load(std::memory_order_acquire)) {
        postRecv();
    }
}

void TlsClient::onSendComplete(DWORD bytes, bool ok) {
    pendingOps_.fetch_sub(1, std::memory_order_acq_rel);
    if (!ok) {
        noteReason("send failed (WSA error " + std::to_string(WSAGetLastError()) + ")");
        close();
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    sendInFlight_ = false;
    sendOffset_ += bytes;
    if (sendOffset_ >= sendBuffer_.size()) {
        sendBuffer_.clear();
        sendOffset_ = 0;
    } else if (sendOffset_ > sendBuffer_.size() / 2) {
        sendBuffer_.erase(sendBuffer_.begin(),
                          sendBuffer_.begin() + static_cast<std::ptrdiff_t>(sendOffset_));
        sendOffset_ = 0;
    }
    flushLocked();
}

void TlsClient::send(const proto::Bytes& frame) {
    if (closed_.load(std::memory_order_acquire)) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!tls_.writePlain(frame.data(), frame.size())) {
        spdlog::debug("send failed: {}", tls_.lastError());
        return;
    }
    flushLocked();
}

void TlsClient::flushLocked() {
    tls_.drainEncrypted(sendBuffer_);

    if (sendInFlight_ || closed_.load(std::memory_order_acquire)) {
        return;
    }
    if (sendOffset_ >= sendBuffer_.size()) {
        return;
    }

    ZeroMemory(static_cast<OVERLAPPED*>(&sendOp_), sizeof(OVERLAPPED));
    WSABUF buffer;
    buffer.buf = sendBuffer_.data() + sendOffset_;
    buffer.len = static_cast<ULONG>(sendBuffer_.size() - sendOffset_);

    sendInFlight_ = true;
    pendingOps_.fetch_add(1, std::memory_order_acq_rel);

    const int rc = WSASend(socket_, &buffer, 1, nullptr, 0, &sendOp_, nullptr);
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        sendInFlight_ = false;
        pendingOps_.fetch_sub(1, std::memory_order_acq_rel);
        noteReason(lastSocketError("WSASend"));
        // 플래그만 세우면 대기 중인 WSARecv 가 영영 완료되지 않아 워커가 갇힌다.
        // close() 가 소켓을 닫아 그 작업을 실패로 완료시킨다.
        close();
    }
}

void TlsClient::close() {
    if (closed_.exchange(true)) {
        return;
    }
    // shutdown 보다 먼저 기록해야 한다. 우리가 끊으면 대기 중이던 WSARecv 가
    // 0바이트로 완료되는데, 그것까지 "상대가 끊었다" 로 보고하면 안 된다.
    // 오류 때문에 들어온 경로라면 이미 기록된 사유가 그대로 유지된다.
    noteReason("closed by this client");

    if (socket_ != INVALID_SOCKET) {
        ::shutdown(socket_, SD_BOTH);
        ::CancelIoEx(reinterpret_cast<HANDLE>(socket_), nullptr);
    }

    // 대기 중인 작업이 하나도 없으면 취소할 것도 없어서 워커가 완료 포트에서 영영
    // 깨어나지 않는다 (수신을 멈춘 --stall 이 그렇다). 빈 완료를 밀어 넣어 깨운다.
    if (iocp_ != nullptr) {
        ::PostQueuedCompletionStatus(iocp_, 0, kIoKey, nullptr);
    }

    // 핸드셰이크를 기다리는 쪽이 있으면 깨워준다. connect() 가 사유를 읽어 던진다.
    {
        std::lock_guard<std::mutex> lock(handshakeMutex_);
        if (!handshakeDone_) {
            handshakeFailed_ = true;
            handshakeCv_.notify_all();
        }
    }
}

void TlsClient::noteReason(std::string reason) {
    std::lock_guard<std::mutex> lock(reasonMutex_);
    if (closeReason_.empty()) {
        closeReason_ = std::move(reason);
    }
}

std::string TlsClient::closeReason() const {
    std::lock_guard<std::mutex> lock(reasonMutex_);
    return closeReason_.empty() ? "connection closed" : closeReason_;
}

void TlsClient::finish() {
    if (!connected_.exchange(false)) {
        return;
    }
    if (onClosed_) {
        onClosed_(closeReason());
    }
}

void TlsClient::wait() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

}  // namespace heaven::net
