#pragma once

#include <winsock2.h>
// winsock2.h 를 먼저 넣어야 windows.h 가 winsock1 을 끌어오지 않는다.
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "FrameHandler.h"
#include "Framing.h"
#include "IoOperation.h"
#include "Tls.h"

namespace heaven::net {

class TlsServer;

// 브로드캐스트 시 세션들이 공유하는 프레임 바이트열.
using Frame = std::shared_ptr<const proto::Bytes>;

// 세션당 전송 대기 바이트 상한. 소켓을 읽지 않는 클라이언트 때문에
// 큐가 무한정 자라는 것을 막는다. 넘으면 해당 연결을 끊는다.
inline constexpr std::size_t kMaxSendQueueBytes = 1024 * 1024;

class TlsSession : public std::enable_shared_from_this<TlsSession> {
public:
    TlsSession(SOCKET socket, TlsServer& server, SSL_CTX* sslContext, std::string peer);
    ~TlsSession();

    TlsSession(const TlsSession&) = delete;
    TlsSession& operator=(const TlsSession&) = delete;

    void setHandler(std::unique_ptr<FrameHandler> handler) { handler_ = std::move(handler); }

    // 첫 WSARecv 를 게시한다. 실패하면 false.
    bool start();

    // --- IOCP 워커 스레드에서 호출된다 ---
    void onRecvComplete(DWORD bytes, bool ok);
    void onSendComplete(DWORD bytes, bool ok);

    // --- 어느 스레드에서든 호출 가능 ---
    void send(const proto::Bytes& frame);
    void send(const Frame& frame);

    // 소켓만 닫는다. 실제 정리는 대기 중인 작업이 실패로 완료되면서 이뤄진다.
    void closeSocket();

    // 전송 큐가 비는 대로 종료한다. 응답 하나 보내고 끊는 흐름에 쓴다.
    void closeAfterFlush();

    // 이 시점까지만 핸드셰이크 타임아웃이 적용된다.
    // 첫 유효 프레임을 받아들인 핸들러가 호출한다.
    void markAuthenticated() { authenticated_.store(true, std::memory_order_release); }
    bool authenticated() const { return authenticated_.load(std::memory_order_acquire); }

    std::chrono::steady_clock::time_point createdAt() const { return createdAt_; }
    const std::string& peer() const { return peer_; }

private:
    bool postRecv();
    void flushLocked();
    void queuePlainLocked(const proto::Bytes& frame);

    // 누적된 평문에서 완성된 프레임을 뽑는다. mutex_ 를 잡은 채로 호출한다.
    bool extractFramesLocked(std::vector<proto::Bytes>& frames);

    // 닫혔고 대기 작업이 없으면 정리한다. 락을 잡지 않은 상태에서 호출할 것.
    void maybeFinish();

    SOCKET socket_;
    TlsServer& server_;
    std::string peer_;
    std::chrono::steady_clock::time_point createdAt_;
    std::unique_ptr<FrameHandler> handler_;

    IoOperation recvOp_{OpType::Recv};
    IoOperation sendOp_{OpType::Send};

    // 대기 중인 작업이 세션을 살려둔다. 완료 처리 시 지역 변수로 옮겨 받는다.
    std::shared_ptr<TlsSession> recvRef_;
    std::shared_ptr<TlsSession> sendRef_;

    char recvBuffer_[16 * 1024];

    std::mutex mutex_;
    TlsChannel tls_;
    std::vector<unsigned char> plainAccum_;  // 아직 프레임이 안 된 평문
    std::vector<char> sendBuffer_;           // 보낼 암호문
    std::size_t sendOffset_ = 0;
    bool sendInFlight_ = false;

    std::atomic<bool> authenticated_{false};
    std::atomic<bool> closeAfterFlush_{false};
    std::atomic<bool> closed_{false};
    std::atomic<bool> finished_{false};
    std::atomic<int> pendingOps_{0};
};

}  // namespace heaven::net
