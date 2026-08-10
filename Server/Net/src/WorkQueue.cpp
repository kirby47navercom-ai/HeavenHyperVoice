#include "WorkQueue.h"

#include <spdlog/spdlog.h>

#include <utility>

namespace heaven::net {

WorkQueue::WorkQueue(unsigned threads) {
    if (threads == 0) {
        threads = 1;
    }
    workers_.reserve(threads);
    for (unsigned i = 0; i < threads; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
}

WorkQueue::~WorkQueue() {
    stop();
}

void WorkQueue::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            return;
        }
        tasks_.push_back(std::move(task));
    }
    ready_.notify_one();
}

void WorkQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
    }
    ready_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void WorkQueue::workerLoop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });

            // 종료 중이라도 큐에 남은 작업은 끝낸다. 그래야 대기 중인
            // 로그인 요청이 응답 없이 끊기지 않는다.
            if (tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }

        try {
            task();
        } catch (const std::exception& e) {
            spdlog::error("auth task threw: {}", e.what());
        }
    }
}

}  // namespace heaven::net
