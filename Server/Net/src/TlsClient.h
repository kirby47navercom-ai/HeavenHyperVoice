#pragma once

// 클라이언트 쪽 TLS 연결 하나. 서버와 같은 IOCP + 메모리 BIO 방식을 쓴다.
//
// 접속 자체는 블로킹 connect() 로 한다 — 연결이 하나뿐이라 ConnectEx 로 얻을 게 없다.
// 그 뒤 소켓을 완료 포트에 붙이고 WSARecv/WSASend 로 주고받으며,
// 워커 스레드 하나가 완료를 처리한다.
//
// 수신 프레임은 그 워커 스레드에서 콜백으로 전달된다.

#include <winsock2.h>
#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Framing.h"
#include "IoOperation.h"
#include "Tls.h"

namespace heaven::net {

class TlsClient {
public:
    using FrameCallback = std::function<void(const proto::Bytes& body)>;
    using CloseCallback = std::function<void()>;

    TlsClient();
    ~TlsClient();

    TlsClient(const TlsClient&) = delete;
    TlsClient& operator=(const TlsClient&) = delete;

    // 프레임 콜백은 connect 전에 걸어야 한다. 워커 스레드에서 불린다.
    void onFrame(FrameCallback callback) { onFrame_ = std::move(callback); }
    void onClosed(CloseCallback callback) { onClosed_ = std::move(callback); }

    // 접속하고 TLS 핸드셰이크가 끝날 때까지 블록한다. 실패하면 예외.
    void connect(const std::string& host, std::uint16_t port);

    void send(const proto::Bytes& frame);

    // 소켓을 닫는다. 대기 중인 작업이 실패로 완료되며 워커가 빠져나온다.
    void close();

    // 연결이 끝날 때까지 기다린다.
    void wait();

    // 수신 재게시를 멈춘다. 소켓 수신 버퍼가 차고, 결국 상대의 전송이 막힌다.
    // 서버의 전송 큐 상한을 시험하기 위한 것이다.
    void pauseReceiving() { paused_.store(true, std::memory_order_release); }

private:
    void workerLoop();
    bool postRecv();
    void onRecvComplete(DWORD bytes, bool ok);
    void onSendComplete(DWORD bytes, bool ok);
    void flushLocked();
    void finish();

    TlsContext context_;
    SOCKET socket_ = INVALID_SOCKET;
    HANDLE iocp_ = nullptr;

    IoOperation recvOp_{OpType::Recv};
    IoOperation sendOp_{OpType::Send};
    char recvBuffer_[16 * 1024];

    std::mutex mutex_;
    TlsChannel tls_;
    std::vector<unsigned char> plainAccum_;
    std::vector<char> sendBuffer_;
    std::size_t sendOffset_ = 0;
    bool sendInFlight_ = false;

    std::mutex handshakeMutex_;
    std::condition_variable handshakeCv_;
    bool handshakeDone_ = false;
    bool handshakeFailed_ = false;
    std::string handshakeError_;

    std::thread worker_;
    FrameCallback onFrame_;
    CloseCallback onClosed_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> closed_{false};
    std::atomic<bool> paused_{false};
    std::atomic<int> pendingOps_{0};
};

}  // namespace heaven::net
