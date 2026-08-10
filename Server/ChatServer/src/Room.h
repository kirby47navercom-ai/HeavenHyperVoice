#pragma once

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "TlsSession.h"

namespace heaven::chat {

using net::TlsSession;

// 입장한 세션들의 집합. 브로드캐스트는 shared 락만 잡는다.
//
// TlsSession::send 는 방을 다시 만지지 않으므로 순회 중에 호출해도 안전하다.
// 전송 큐가 넘쳐 세션을 끊어야 할 때도 소켓만 닫고, 방에서 빼는 것은
// 나중에 워커 스레드가 처리한다.
class Room {
public:
    // 계정 인덱스에도 함께 넣는다. 같은 계정이 이미 있으면 그 세션을 돌려준다
    // (호출자가 끊는다). 인덱스는 새 세션이 차지한다.
    std::shared_ptr<TlsSession> join(std::uint64_t accountId,
                                     const std::shared_ptr<TlsSession>& session);

    // 계정 인덱스는 아직 이 세션이 주인일 때만 지운다. 그러지 않으면
    // 이미 자리를 넘겨준 옛 세션이 나가면서 새 세션의 인덱스를 지운다.
    void leave(std::uint64_t accountId, const std::shared_ptr<TlsSession>& session);

    void broadcast(const proto::Bytes& frame, const TlsSession* except);
    std::size_t size() const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_set<std::shared_ptr<TlsSession>> sessions_;
    std::unordered_map<std::uint64_t, std::shared_ptr<TlsSession>> byAccount_;
};

}  // namespace heaven::chat
