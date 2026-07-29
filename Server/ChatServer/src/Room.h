#pragma once

#include <memory>
#include <shared_mutex>
#include <unordered_set>

#include "TlsSession.h"

namespace heaven::chat {

using net::Frame;
using net::TlsSession;

// 입장한 세션들의 집합. 브로드캐스트는 shared 락만 잡는다.
//
// TlsSession::send 는 방을 다시 만지지 않으므로 순회 중에 호출해도 안전하다.
// 전송 큐가 넘쳐 세션을 끊어야 할 때도 소켓만 닫고, 방에서 빼는 것은
// 나중에 워커 스레드가 처리한다.
class Room {
public:
    void join(const std::shared_ptr<TlsSession>& session);
    void leave(const std::shared_ptr<TlsSession>& session);
    void broadcast(const Frame& frame, const TlsSession* except);
    std::size_t size() const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_set<std::shared_ptr<TlsSession>> sessions_;
};

}  // namespace heaven::chat
