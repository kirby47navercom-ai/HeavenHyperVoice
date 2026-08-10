#pragma once

// 인증 작업 전용 스레드 풀.
//
// argon2 검증 하나가 Release 기준 약 18.5ms 다. 여기에 DB 왕복까지 더해
// IOCP 워커 스레드에서 처리하면 그 워커가 통째로 멈춘다. 워커가 전부 인증에
// 매달리면 같은 완료 포트를 쓰는 다른 I/O 가 함께 밀린다.
//
// LoginHandler 는 파싱만 하고 이 큐에 넘긴 뒤 즉시 반환한다.
// 응답 전송은 인증 스레드가 담당한다 (TlsSession::send 는 스레드 안전).

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace heaven::net {

// 스레드당 대기 상한. 큐가 무제한이면 접속만 계속 열어 로그인을 밀어넣는 것으로
// 메모리를 채울 수 있다 (작업 하나가 세션을 shared_ptr 로 붙잡는다).
// argon2 하나가 ~19ms 라 스레드당 64건이면 이미 1초가 넘게 밀린 상태다.
inline constexpr std::size_t kMaxPendingPerThread = 64;

class WorkQueue {
public:
    explicit WorkQueue(unsigned threads);
    ~WorkQueue();

    WorkQueue(const WorkQueue&) = delete;
    WorkQueue& operator=(const WorkQueue&) = delete;

    // 어느 스레드에서든 호출 가능.
    // 큐가 가득 찼거나 stop() 된 뒤면 작업을 버리고 false. 호출자가 거절을 알린다.
    bool submit(std::function<void()> task);

    // 남은 작업을 모두 처리하고 스레드를 정리한다. 여러 번 호출해도 안전하다.
    void stop();

    unsigned threadCount() const { return static_cast<unsigned>(workers_.size()); }

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> tasks_;
    std::size_t maxPending_ = 0;
    bool stopping_ = false;
};

}  // namespace heaven::net
