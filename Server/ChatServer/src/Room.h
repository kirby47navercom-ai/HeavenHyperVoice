#pragma once

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "TlsSession.h"

namespace heaven::chat {

using net::TlsSession;

// 입장한 세션들의 집합이자 온라인 명부. 브로드캐스트는 shared 락만 잡는다.
//
// TlsSession::send 는 방을 다시 만지지 않으므로 순회 중에 호출해도 안전하다.
// 전송 큐가 넘쳐 세션을 끊어야 할 때도 소켓만 닫고, 방에서 빼는 것은
// 나중에 워커 스레드가 처리한다.
//
// 닉네임 색인을 함께 들고 있는 이유는 파티 초대가 닉네임으로 오기 때문이다.
// 계정 번호는 클라이언트가 알 방법이 없다.
class Room {
public:
    // 계정 인덱스에도 함께 넣는다. 같은 계정이 이미 있으면 그 세션을 돌려준다
    // (호출자가 끊는다). 인덱스는 새 세션이 차지한다.
    std::shared_ptr<TlsSession> join(std::uint64_t accountId, const std::string& nickname,
                                     const std::shared_ptr<TlsSession>& session);

    // 계정 인덱스는 아직 이 세션이 주인일 때만 지운다. 그러지 않으면
    // 이미 자리를 넘겨준 옛 세션이 나가면서 새 세션의 인덱스를 지운다.
    void leave(std::uint64_t accountId, const std::shared_ptr<TlsSession>& session);

    void broadcast(const proto::Bytes& frame, const TlsSession* except);

    // 지정한 계정들에게만 보낸다. 접속 중이 아닌 계정은 조용히 건너뛴다.
    void sendToAccounts(const std::vector<std::uint64_t>& accountIds, const proto::Bytes& frame);

    // 접속 중이면 그 세션, 아니면 nullptr.
    std::shared_ptr<TlsSession> findAccount(std::uint64_t accountId) const;

    // 닉네임으로 계정을 찾는다. 접속 중이 아니면 0.
    // 대소문자와 앞뒤 공백은 호출자가 이미 다듬어 넘긴다.
    std::uint64_t accountByNickname(const std::string& nickname) const;

    // 지금 붙어 있는 계정 전부. 생존 신호를 주기적으로 갱신하는 데 쓴다.
    std::vector<std::uint64_t> onlineAccounts() const;

    std::size_t size() const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_set<std::shared_ptr<TlsSession>> sessions_;
    std::unordered_map<std::uint64_t, std::shared_ptr<TlsSession>> byAccount_;

    // 닉네임 -> 계정. 닉네임은 DB 에서 전역 유일하므로 겹치지 않는다.
    std::unordered_map<std::string, std::uint64_t> byNickname_;

    // 나갈 때 어떤 닉네임을 지워야 하는지 알아야 한다.
    std::unordered_map<std::uint64_t, std::string> nicknames_;
};

}  // namespace heaven::chat
