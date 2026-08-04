#pragma once

// 계정별 "지금 어디에 접속해 있나" 를 Redis 에 기록한다.
//
// 서버가 한 종류뿐인 지금은 Room 의 계정 인덱스만으로도 중복 로그인을 막을 수 있다.
// 이 레지스트리가 필요해지는 건 클라이언트가 직접 붙는 프로세스가 하나 더 생길 때다
// (음성 서버). 그때 계정별 세션을 프로세스 경계 너머로 봐야 한다.
//
// 담기는 값은 언제 사라져도 되는 것들이다. Redis 가 죽거나 비워져도
// 채팅은 계속 되고, 중복 감지만 잠시 약해진다.

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "RedisClient.h"

namespace heaven::chat {

// 등록의 수명. 서버가 죽으면 이 시간 뒤 기록이 사라져 유령 계정이 남지 않는다.
inline constexpr std::chrono::seconds kSessionTtl{60};

// 갱신 주기. TTL 보다 넉넉히 짧아야 한 번 놓쳐도 만료되지 않는다.
inline constexpr std::chrono::seconds kSessionRefreshInterval{20};

// 키의 이전 주인.
struct SessionOwner {
    std::string serverId;
    std::string sessionId;
};

class SessionRegistry {
public:
    SessionRegistry(net::RedisClient& redis, std::string serverId,
                    std::chrono::seconds entryTtl);

    // 계정을 자기 것으로 등록하고 **이전 주인을 돌려준다** (후접속 우선).
    // 덮어쓰기와 조회가 한 번에 일어나야 한다. 따로 하면 그 사이에 다른 로그인이
    // 끼어들어 둘 다 자기가 주인이라고 믿는 상태가 될 수 있다.
    //
    // 이전 주인이 없거나 Redis 를 못 쓰면 nullopt.
    std::optional<SessionOwner> claim(std::uint64_t accountId, const std::string& sessionId);

    // 아직 자기 것일 때만 지운다. 이 확인이 없으면 나중에 들어온 주인의
    // 등록을 지워버릴 수 있다.
    void release(std::uint64_t accountId, const std::string& sessionId);

    // 이 서버가 들고 있는 등록들의 TTL 을 늘린다. 주기적으로 호출한다.
    //
    // EXPIRE 가 아니라 SET 을 쓴다. EXPIRE 는 키가 없으면 아무것도 하지 않아서
    // Redis 가 재시작으로 비워지면 영영 복구되지 않는다. SET 이면 다음 주기에
    // 스스로 다시 채워진다.
    void refreshAll();

    bool available() { return redis_.connected(); }
    const std::string& serverId() const { return serverId_; }
    std::size_t heldCount();

private:
    std::string keyFor(std::uint64_t accountId) const;
    std::string valueFor(const std::string& sessionId) const;
    std::optional<SessionOwner> parse(const std::string& value) const;

    net::RedisClient& redis_;
    std::string serverId_;
    std::chrono::seconds entryTtl_;

    // 이 서버가 등록한 것들. 하트비트가 훑는다.
    std::mutex mutex_;
    std::unordered_map<std::uint64_t, std::string> held_;
};

}  // namespace heaven::chat
