#include "TlsServer.h"

#include <spdlog/spdlog.h>
#include <ws2tcpip.h>

#include <stdexcept>
#include <utility>

namespace heaven::net {

namespace {

// 리슨 소켓의 완료는 세션과 구분해야 하므로 별도 완료 키를 쓴다.
constexpr ULONG_PTR kAcceptKey = 1;
constexpr ULONG_PTR kStopKey = 2;

std::string lastSocketError(const char* what) {
    return std::string(what) + " failed: WSA error " + std::to_string(WSAGetLastError());
}

std::string describePeer(SOCKET socket) {
    sockaddr_in address{};
    int length = sizeof(address);
    if (::getpeername(socket, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return "unknown";
    }
    char host[INET_ADDRSTRLEN] = {};
    ::inet_ntop(AF_INET, &address.sin_addr, host, sizeof(host));
    return std::string(host) + ":" + std::to_string(ntohs(address.sin_port));
}

}  // namespace

TlsServer::TlsServer(TlsServerOptions options, TlsContext& tls, HandlerFactory factory)
    : options_(options), tls_(tls), factory_(std::move(factory)) {
    WSADATA wsaData;
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }

    iocp_ = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (iocp_ == nullptr) {
        ::WSACleanup();
        throw std::runtime_error("CreateIoCompletionPort failed");
    }

    try {
        createListenSocket();
        loadAcceptEx();
    } catch (...) {
        if (listenSocket_ != INVALID_SOCKET) ::closesocket(listenSocket_);
        ::CloseHandle(iocp_);
        ::WSACleanup();
        throw;
    }
}

TlsServer::~TlsServer() {
    if (listenSocket_ != INVALID_SOCKET) {
        ::closesocket(listenSocket_);
    }
    if (pendingAccept_ != INVALID_SOCKET) {
        ::closesocket(pendingAccept_);
    }
    if (iocp_ != nullptr) {
        ::CloseHandle(iocp_);
    }
    ::WSACleanup();
}

void TlsServer::createListenSocket() {
    listenSocket_ =
        ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (listenSocket_ == INVALID_SOCKET) {
        throw std::runtime_error(lastSocketError("WSASocketW"));
    }

    BOOL reuse = TRUE;
    ::setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                 sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(options_.port);

    if (::bind(listenSocket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) ==
        SOCKET_ERROR) {
        throw std::runtime_error(lastSocketError("bind"));
    }
    if (::listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
        throw std::runtime_error(lastSocketError("listen"));
    }
    if (::CreateIoCompletionPort(reinterpret_cast<HANDLE>(listenSocket_), iocp_, kAcceptKey, 0) ==
        nullptr) {
        throw std::runtime_error("failed to associate listen socket with the IOCP");
    }
}

void TlsServer::loadAcceptEx() {
    GUID guid = WSAID_ACCEPTEX;
    DWORD bytes = 0;
    if (::WSAIoctl(listenSocket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                   &acceptEx_, sizeof(acceptEx_), &bytes, nullptr, nullptr) == SOCKET_ERROR) {
        throw std::runtime_error(lastSocketError("WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)"));
    }
}

bool TlsServer::postAccept() {
    if (stopping_.load(std::memory_order_acquire)) {
        return false;
    }

    pendingAccept_ =
        ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (pendingAccept_ == INVALID_SOCKET) {
        spdlog::error("{}", lastSocketError("WSASocketW for accept"));
        return false;
    }

    ZeroMemory(static_cast<OVERLAPPED*>(&acceptOp_), sizeof(OVERLAPPED));

    // dwReceiveDataLength 를 0 으로 준다. 0 이 아니면 첫 데이터가 올 때까지
    // accept 가 완료되지 않아, 침묵하는 클라이언트가 accept 를 붙잡는다.
    DWORD received = 0;
    const BOOL ok = acceptEx_(listenSocket_, pendingAccept_, acceptBuffer_, 0, kAddrLen, kAddrLen,
                              &received, &acceptOp_);
    if (!ok && WSAGetLastError() != ERROR_IO_PENDING) {
        spdlog::error("{}", lastSocketError("AcceptEx"));
        ::closesocket(pendingAccept_);
        pendingAccept_ = INVALID_SOCKET;
        return false;
    }
    return true;
}

void TlsServer::onAcceptComplete(bool ok) {
    const SOCKET accepted = pendingAccept_;
    pendingAccept_ = INVALID_SOCKET;

    if (!ok || accepted == INVALID_SOCKET) {
        if (accepted != INVALID_SOCKET) {
            ::closesocket(accepted);
        }
        postAccept();
        return;
    }

    // AcceptEx 로 받은 소켓은 리슨 소켓의 속성을 물려받아야 getpeername 등이 동작한다.
    ::setsockopt(accepted, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                 reinterpret_cast<const char*>(&listenSocket_), sizeof(listenSocket_));

    BOOL noDelay = TRUE;
    ::setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay),
                 sizeof(noDelay));

    std::shared_ptr<TlsSession> session;
    try {
        session = std::make_shared<TlsSession>(accepted, *this, tls_.native(),
                                               describePeer(accepted));
        session->setHandler(factory_(*session));
    } catch (const std::exception& e) {
        spdlog::error("failed to create session: {}", e.what());
        ::closesocket(accepted);
        postAccept();
        return;
    }

    // 세션 소켓을 완료 포트에 붙인다. 완료 키는 세션 포인터.
    if (::CreateIoCompletionPort(reinterpret_cast<HANDLE>(accepted), iocp_,
                                 reinterpret_cast<ULONG_PTR>(session.get()), 0) == nullptr) {
        spdlog::error("failed to associate the accepted socket with the IOCP");
        postAccept();
        return;
    }

    {
        std::unique_lock<std::shared_mutex> lock(sessionsMutex_);
        sessions_.insert(session);
    }

    spdlog::debug("accepted: {}", session->peer());
    if (!session->start()) {
        std::unique_lock<std::shared_mutex> lock(sessionsMutex_);
        sessions_.erase(session);
    }

    postAccept();
}

void TlsServer::workerLoop() {
    for (;;) {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* overlapped = nullptr;

        const BOOL ok = ::GetQueuedCompletionStatus(iocp_, &bytes, &key, &overlapped, INFINITE);

        if (key == kStopKey) {
            return;
        }
        if (overlapped == nullptr) {
            if (stopping_.load(std::memory_order_acquire)) {
                return;
            }
            continue;
        }

        if (key == kAcceptKey) {
            onAcceptComplete(ok == TRUE);
            continue;
        }

        auto* session = reinterpret_cast<TlsSession*>(key);
        auto* operation = static_cast<IoOperation*>(overlapped);
        switch (operation->type) {
            case OpType::Recv:
                session->onRecvComplete(bytes, ok == TRUE);
                break;
            case OpType::Send:
                session->onSendComplete(bytes, ok == TRUE);
                break;
        }
    }
}

void TlsServer::timerLoop() {
    // IOCP 에는 타이머가 없다. 1초마다 훑어 첫 프레임이 늦은 연결을 끊는다.
    while (!stopping_.load(std::memory_order_acquire)) {
        ::Sleep(1000);

        const auto now = std::chrono::steady_clock::now();
        std::vector<std::shared_ptr<TlsSession>> silent;
        std::vector<std::shared_ptr<TlsSession>> overstayed;
        {
            std::shared_lock<std::shared_mutex> lock(sessionsMutex_);
            for (const auto& session : sessions_) {
                const auto age = now - session->createdAt();
                if (!session->authenticated() && age > options_.handshakeTimeout) {
                    silent.push_back(session);
                } else if (options_.maxSessionLifetime.count() > 0 &&
                           age > options_.maxSessionLifetime) {
                    overstayed.push_back(session);
                }
            }
        }

        for (const auto& session : silent) {
            spdlog::warn("{}: handshake timed out after {}s", session->peer(),
                         options_.handshakeTimeout.count());
            session->closeSocket();
        }
        for (const auto& session : overstayed) {
            spdlog::info("{}: closed after {}s (session lifetime cap)", session->peer(),
                         options_.maxSessionLifetime.count());
            session->closeSocket();
        }
    }
}

void TlsServer::run() {
    workerCount_ = options_.workerThreads;
    if (workerCount_ == 0) {
        workerCount_ = std::thread::hardware_concurrency();
        if (workerCount_ == 0) {
            workerCount_ = 2;
        }
    }

    if (!postAccept()) {
        throw std::runtime_error("failed to post the initial AcceptEx");
    }

    workers_.reserve(workerCount_);
    for (unsigned i = 0; i < workerCount_; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
    timerThread_ = std::thread([this] { timerLoop(); });

    for (std::thread& worker : workers_) {
        worker.join();
    }
    if (timerThread_.joinable()) {
        timerThread_.join();
    }
    closeAllSessions();
}

void TlsServer::stop() {
    if (stopping_.exchange(true)) {
        return;
    }

    // 리슨 소켓을 닫아 새 연결을 막고, 워커를 하나씩 깨워 종료시킨다.
    if (listenSocket_ != INVALID_SOCKET) {
        ::closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }
    for (unsigned i = 0; i < workerCount_; ++i) {
        ::PostQueuedCompletionStatus(iocp_, 0, kStopKey, nullptr);
    }
}

void TlsServer::closeAllSessions() {
    std::vector<std::shared_ptr<TlsSession>> all;
    {
        std::shared_lock<std::shared_mutex> lock(sessionsMutex_);
        all.assign(sessions_.begin(), sessions_.end());
    }
    for (const auto& session : all) {
        session->closeSocket();
    }
}

void TlsServer::onSessionClosed(const std::shared_ptr<TlsSession>& session) {
    std::unique_lock<std::shared_mutex> lock(sessionsMutex_);
    sessions_.erase(session);
}

}  // namespace heaven::net
