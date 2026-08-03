#include "TlsSession.h"

#include <spdlog/spdlog.h>

#include <utility>

#include "TlsServer.h"

namespace heaven::net {

TlsSession::TlsSession(SOCKET socket, TlsServer& server, SSL_CTX* sslContext, std::string peer)
    : socket_(socket),
      server_(server),
      peer_(std::move(peer)),
      createdAt_(std::chrono::steady_clock::now()),
      tls_(sslContext, TlsRole::Server) {}

TlsSession::~TlsSession() {
    if (socket_ != INVALID_SOCKET) {
        ::closesocket(socket_);
    }
}

bool TlsSession::start() {
    return postRecv();
}

void TlsSession::closeSocket() {
    if (closed_.exchange(true)) {
        return;
    }
    // 대기 중인 작업이 즉시 실패로 완료되고, 그 완료 처리에서 정리가 이뤄진다.
    ::shutdown(socket_, SD_BOTH);
    ::CancelIoEx(reinterpret_cast<HANDLE>(socket_), nullptr);
}

void TlsSession::closeAfterFlush() {
    closeAfterFlush_.store(true, std::memory_order_release);

    bool drained = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        drained = !sendInFlight_ && sendOffset_ >= sendBuffer_.size();
    }
    if (drained) {
        closeSocket();
    }
}

bool TlsSession::postRecv() {
    if (closed_.load(std::memory_order_acquire)) {
        return false;
    }

    recvRef_ = shared_from_this();
    ZeroMemory(static_cast<OVERLAPPED*>(&recvOp_), sizeof(OVERLAPPED));

    WSABUF buffer;
    buffer.buf = recvBuffer_;
    buffer.len = static_cast<ULONG>(sizeof(recvBuffer_));

    DWORD flags = 0;
    pendingOps_.fetch_add(1, std::memory_order_acq_rel);

    const int rc = WSARecv(socket_, &buffer, 1, nullptr, &flags, &recvOp_, nullptr);
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        pendingOps_.fetch_sub(1, std::memory_order_acq_rel);
        recvRef_.reset();
        closeSocket();
        return false;
    }
    return true;
}

void TlsSession::onRecvComplete(DWORD bytes, bool ok) {
    // 지역 변수로 옮겨 이 함수가 끝날 때까지 세션을 살려둔다.
    const std::shared_ptr<TlsSession> keepAlive = std::move(recvRef_);
    pendingOps_.fetch_sub(1, std::memory_order_acq_rel);

    if (!ok || bytes == 0) {
        closeSocket();
        maybeFinish();
        return;
    }

    std::vector<proto::Bytes> frames;
    bool fatal = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        tls_.feedEncrypted(recvBuffer_, bytes);
        if (!tls_.pump(plainAccum_)) {
            spdlog::debug("{}: tls closed: {}", peer_, tls_.lastError());
            fatal = true;
        } else if (!extractFramesLocked(frames)) {
            fatal = true;
        }
        // 핸드셰이크 응답 등 나갈 게 있으면 내보낸다.
        flushLocked();
    }

    // 프레임 처리는 세션 락 밖에서 한다. 핸들러가 방/등록부를 자유롭게 만질 수 있고,
    // 락 순서를 신경 쓸 필요가 없어진다.
    if (!fatal && handler_ != nullptr) {
        for (const proto::Bytes& body : frames) {
            if (!handler_->onFrame(*this, body)) {
                fatal = true;
                break;
            }
        }
    }

    if (fatal) {
        // 거절 통지 같은 마지막 응답이 큐에 있을 수 있으므로 비우고 끊는다.
        closeAfterFlush();
        maybeFinish();
        return;
    }

    postRecv();
    maybeFinish();
}

void TlsSession::onSendComplete(DWORD bytes, bool ok) {
    const std::shared_ptr<TlsSession> keepAlive = std::move(sendRef_);
    pendingOps_.fetch_sub(1, std::memory_order_acq_rel);

    if (!ok) {
        closeSocket();
        maybeFinish();
        return;
    }

    bool drained = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sendInFlight_ = false;
        sendOffset_ += bytes;

        if (sendOffset_ >= sendBuffer_.size()) {
            sendBuffer_.clear();
            sendOffset_ = 0;
        } else if (sendOffset_ > sendBuffer_.size() / 2) {
            // 앞부분이 절반 넘게 소비되면 압축한다. 매번 erase 하면 O(n^2) 이 된다.
            sendBuffer_.erase(sendBuffer_.begin(),
                              sendBuffer_.begin() + static_cast<std::ptrdiff_t>(sendOffset_));
            sendOffset_ = 0;
        }

        flushLocked();
        drained = !sendInFlight_ && sendOffset_ >= sendBuffer_.size();
    }

    if (drained && closeAfterFlush_.load(std::memory_order_acquire)) {
        closeSocket();
    }

    maybeFinish();
}

void TlsSession::send(const Frame& frame) {
    send(*frame);
}

void TlsSession::send(const proto::Bytes& frame) {
    if (closed_.load(std::memory_order_acquire)) {
        return;
    }

    bool overflow = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sendBuffer_.size() - sendOffset_ >= kMaxSendQueueBytes) {
            overflow = true;
        } else if (!tls_.writePlain(frame.data(), frame.size())) {
            spdlog::warn("{}: {}", peer_, tls_.lastError());
        } else {
            // 암호문을 꺼내 큐에 붙이는 것은 flushLocked 가 한다.
            flushLocked();
        }
    }

    if (overflow) {
        spdlog::warn("{}: send queue over {} bytes, disconnecting slow client", peer_,
                     kMaxSendQueueBytes);
        // 소켓만 닫는다. 공유 자원을 만지지 않으므로 어떤 순회 도중에 호출해도 안전하고,
        // 대기 중인 WSARecv 가 실패로 완료되면서 정리가 진행된다.
        closeSocket();
    }
}

void TlsSession::flushLocked() {
    tls_.drainEncrypted(sendBuffer_);

    if (sendInFlight_ || closed_.load(std::memory_order_acquire)) {
        return;
    }
    if (sendOffset_ >= sendBuffer_.size()) {
        return;
    }

    sendRef_ = shared_from_this();
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
        sendRef_.reset();
        closeSocket();
    }
}

bool TlsSession::extractFramesLocked(std::vector<proto::Bytes>& frames) {
    for (;;) {
        if (plainAccum_.size() < proto::kHeaderBytes) {
            return true;
        }

        const std::uint32_t length = proto::decodeLength(plainAccum_.data());
        if (length == 0 || length > proto::kMaxBodyBytes) {
            spdlog::warn("{}: frame length out of range: {}", peer_, length);
            return false;
        }
        if (plainAccum_.size() < proto::kHeaderBytes + length) {
            return true;  // 아직 다 안 왔다
        }

        const auto bodyBegin =
            plainAccum_.begin() + static_cast<std::ptrdiff_t>(proto::kHeaderBytes);
        frames.emplace_back(bodyBegin, bodyBegin + static_cast<std::ptrdiff_t>(length));
        plainAccum_.erase(plainAccum_.begin(), bodyBegin + static_cast<std::ptrdiff_t>(length));
    }
}

void TlsSession::maybeFinish() {
    if (!closed_.load(std::memory_order_acquire)) {
        return;
    }
    if (pendingOps_.load(std::memory_order_acquire) != 0) {
        return;
    }
    if (finished_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    // 등록부에서 빠지면 마지막 참조가 사라질 수 있으므로 붙잡아 둔다.
    const std::shared_ptr<TlsSession> self = shared_from_this();
    if (handler_ != nullptr) {
        handler_->onClosed(*this);
    }
    server_.onSessionClosed(self);
}

}  // namespace heaven::net
