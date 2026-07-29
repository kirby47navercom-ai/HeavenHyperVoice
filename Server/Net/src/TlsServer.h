#pragma once

#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "Tls.h"
#include "TlsSession.h"

namespace heaven::net {

// TCP 연결부터 첫 유효 프레임까지 허용하는 시간.
// 붙어놓고 아무것도 보내지 않는 연결이 소켓을 점유하는 것을 막는다 (slowloris).
inline constexpr std::chrono::seconds kHandshakeTimeout{10};

using HandlerFactory = std::function<std::unique_ptr<FrameHandler>(TlsSession&)>;

struct TlsServerOptions {
    std::uint16_t port = 9000;
    unsigned workerThreads = 0;  // 0 이면 하드웨어 동시성에서 정한다
    std::chrono::seconds handshakeTimeout = kHandshakeTimeout;
};

class TlsServer {
public:
    TlsServer(TlsServerOptions options, TlsContext& tls, HandlerFactory factory);
    ~TlsServer();

    TlsServer(const TlsServer&) = delete;
    TlsServer& operator=(const TlsServer&) = delete;

    void run();   // 워커를 띄우고 stop() 될 때까지 블록한다
    void stop();  // 다른 스레드/콘솔 핸들러에서 호출 가능

    // TlsSession 이 정리를 마칠 때 호출한다.
    void onSessionClosed(const std::shared_ptr<TlsSession>& session);

private:
    void createListenSocket();
    void loadAcceptEx();
    bool postAccept();
    void onAcceptComplete(bool ok);
    void workerLoop();
    void timerLoop();
    void closeAllSessions();

    TlsServerOptions options_;
    TlsContext& tls_;
    HandlerFactory factory_;

    HANDLE iocp_ = nullptr;
    SOCKET listenSocket_ = INVALID_SOCKET;
    SOCKET pendingAccept_ = INVALID_SOCKET;
    LPFN_ACCEPTEX acceptEx_ = nullptr;

    // AcceptEx 는 로컬/원격 주소를 담을 여유 버퍼를 요구한다.
    static constexpr DWORD kAddrLen = sizeof(SOCKADDR_IN) + 16;
    char acceptBuffer_[kAddrLen * 2]{};
    IoOperation acceptOp_{OpType::Recv};  // 완료 키로 구분하므로 type 은 쓰이지 않는다

    unsigned workerCount_ = 0;
    std::vector<std::thread> workers_;
    std::thread timerThread_;
    std::atomic<bool> stopping_{false};

    // 수립된 모든 세션. 핸드셰이크 타임아웃 검사와 수명 관리에 쓴다.
    mutable std::shared_mutex sessionsMutex_;
    std::unordered_set<std::shared_ptr<TlsSession>> sessions_;
};

}  // namespace heaven::net
