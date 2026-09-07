#pragma once

// 플레이어 파티(같이 다니는 그룹). 포켓몬 파티(data::Character::party)와는 다른 것이다.
//
// 상태를 Redis 에 두는 이유는 서버가 셋이기 때문이다. Chat / Field / Instance 는
// 서로를 모르는 별개 프로세스이고 공유 자원은 MySQL 과 Redis 뿐인데, 파티는 세션
// 수명이라 DB 에 남길 것이 아니다. TTL 로 정리까지 따라온다.
//
// **쓰는 것은 ChatServer 하나다.** 다른 서버는 읽기만 한다. 쓰기가 한 곳이면
// 경쟁 조건도 한 곳에만 생긴다.
//
// 여러 키를 함께 바꾸는 연산은 전부 EVAL 로 보낸다. Lua 가 문자열을 돌려주므로
// RedisClient::commandForString 으로 그대로 받을 수 있다 — 배열 응답을 다루는
// 코드를 새로 만들지 않아도 된다.
//
// 단일 노드 Redis(Memurai, localhost)를 전제로 키를 스크립트 안에서 만든다.
// 클러스터로 가면 KEYS[] 로 넘기도록 고쳐야 한다.
//
// 키 구성
//   party:seq                    INCR 로 파티 번호를 뽑는다
//   party:<id>                   LIST  accountId, 가입순. head 가 파티장
//   party:names:<id>             HASH  accountId -> 닉네임
//   party:member:<accountId>     STR   partyId. **생존 신호를 겸한다** (TTL 30)
//   party:<id>:enter:<type>      STR   입장 허가권 (TTL 60)
//   party:<id>:room:<type>       STR   방 고정 (TTL 300)
//   party:invite:<accountId>     HASH  partyId -> 초대한 사람 accountId (TTL 60)

#include <cstdint>
#include <string>
#include <vector>

namespace heaven::net {
class RedisClient;
}

namespace heaven::party {

// 파티 정원. 포켓몬 파티 상한(data::kMaxPartySize = 3)과 무관하다.
inline constexpr std::size_t kMaxMembers = 4;

// 멤버 생존 신호의 수명.
//
// 레벨을 옮길 때 채팅 연결이 끊겼다가 다시 붙는다(AUEPlayerController 소유라
// travel 을 못 견딘다). 끊기자마자 파티에서 빼면 다 같이 인스턴스로 들어가려는
// 그 동작이 파티를 해산시킨다. 그래서 유예를 둔다 — 이동은 보통 1~3초이고,
// 진짜 로그아웃은 이 시간이면 걸러진다.
inline constexpr int kMemberTtlSeconds = 30;

// 파티장이 연 입장 허가권의 수명. 이 안에 못 들어오면 낙오다.
inline constexpr int kEnterTtlSeconds = 60;

// 방 고정의 수명. 먼저 들어간 사람이 이 시간 안에 있으면 뒷사람도 같은 방이다.
inline constexpr int kRoomTtlSeconds = 300;

inline constexpr int kInviteTtlSeconds = 60;

// 파티 목록 자체의 백스톱 수명. 보통은 마지막 멤버가 사라질 때 lazy sweep 이
// 지우지만, 아무도 읽지 않는 파티가 영원히 남지 않게 상한을 둔다.
inline constexpr int kPartyTtlSeconds = 86400;

struct PartyMember {
    std::uint64_t accountId = 0;
    std::string nickname;
};

// 한 시점의 파티. members 는 가입순이고 [0] 이 파티장이다.
struct PartyView {
    std::uint64_t id = 0;
    std::vector<PartyMember> members;

    bool valid() const { return id != 0; }
    std::uint64_t leader() const { return members.empty() ? 0 : members.front().accountId; }
    bool isLeader(std::uint64_t accountId) const {
        return accountId != 0 && leader() == accountId;
    }
    bool contains(std::uint64_t accountId) const;
    std::vector<std::uint64_t> accountIds() const;
};

enum class InviteResult { Ok, Full, TargetBusy, Failed };
enum class AcceptResult { Ok, NoInvite, Full, AlreadyInParty, Gone, Failed };

// Redis 를 파티 저장소로 쓴다. RedisClient 는 소유하지 않는다 — main 이 갖고
// 있고 이 객체보다 오래 산다.
//
// Redis 가 없거나 끊겨 있으면 모든 조회가 "파티 없음" 으로 떨어지고 변경은
// 실패한다. 파티 기능만 죽고 플레이는 계속된다.
class PartyStore {
public:
    explicit PartyStore(net::RedisClient& redis) : redis_(redis) {}

    // 이 계정이 속한 파티. 없으면 id 가 0 이다.
    // 읽으면서 생존 신호가 끊긴 멤버를 목록에서 제거한다(lazy sweep). 주기적으로
    // 도는 청소 작업을 두지 않는 이유는, 그것이 또 하나의 "죽으면 조용히 쌓이는"
    // 구성요소가 되기 때문이다.
    PartyView find(std::uint64_t accountId);

    // 파티 번호로 직접 읽는다. 누가 빠진 뒤 남은 사람들에게 알릴 때 쓴다 —
    // 그때는 빠진 사람의 계정으로 되짚을 수 없다.
    PartyView findById(std::uint64_t partyId);

    // 이 계정이 속한 파티 번호만. 없으면 0.
    std::uint64_t partyIdOf(std::uint64_t accountId);

    // 초대장을 남긴다. inviter 에게 파티가 없으면 그를 파티장으로 하는 파티를
    // 먼저 만든다 — 혼자짜리 파티는 아무 일도 하지 않으므로 "파티 만들기" 를
    // 따로 두지 않는다.
    //
    // 파티장만 초대할 수 있게 하지는 않았다. 넷뿐인 파티에서 굳이 막을 이유가 없다.
    InviteResult invite(std::uint64_t inviter, const std::string& inviterNickname,
                        std::uint64_t target, std::uint64_t& outPartyId);

    // 초대 수락. 성공하면 out 에 갱신된 파티가 담긴다.
    AcceptResult accept(std::uint64_t accountId, const std::string& nickname,
                        std::uint64_t partyId, PartyView& out);

    // 초대장을 버린다.
    void decline(std::uint64_t accountId, std::uint64_t partyId);

    // 탈퇴와 추방이 같은 연산이다. 파티장이 빠지면 목록의 다음 사람이 자동으로
    // 파티장이 된다 — 위임 코드가 따로 없다.
    // 반환은 빠지기 전에 속해 있던 파티 번호. 없었으면 0.
    std::uint64_t leave(std::uint64_t accountId);

    // 접속 중임을 알린다. 생존 신호와 파티 목록의 수명을 늘린다.
    void touch(std::uint64_t accountId);

    // 파티장이 인스턴스 입장을 연다.
    bool openEntry(std::uint64_t partyId, std::uint32_t instanceType);

    // 지금 그 인스턴스로 들어가도 되는가. 개별 입장 금지의 실제 집행 지점이다.
    bool entryOpen(std::uint64_t partyId, std::uint32_t instanceType);

    // 파티가 쓸 방을 정한다. 이미 정해져 있으면 그 번호를, 아니면 fallbackRoomId 를
    // 심고 그대로 돌려준다. 동시에 들어와도 하나만 남는다.
    std::uint32_t claimRoom(std::uint64_t partyId, std::uint32_t instanceType,
                            std::uint32_t fallbackRoomId);

private:
    // EVAL 한 번. 실패하면 빈 문자열.
    std::string eval(const char* script, const std::vector<std::string>& args);

    net::RedisClient& redis_;
};

}  // namespace heaven::party
