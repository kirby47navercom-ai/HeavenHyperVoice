#pragma once

// 누가 어느 인스턴스 방에 있는지. Redis 에 둔다.
//
// 인스턴스 채팅이 필요해서 생겼다. ChatServer 는 방 배정을 모르고
// (InstanceServer 가 한다), 두 프로세스가 서로를 부를 방법이 없다. 파티와 같은
// 이유로 공유 자원인 Redis 를 통한다.
//
// **쓰는 것은 InstanceServer 하나다.** ChatServer 는 읽기만 한다.
//
// 단일 노드 Redis 를 전제로 키를 스크립트 안에서 만든다 (PartyStore 와 같다).
//
// 키 구성
//   player:instance:<accountId>   STR  "<type>:<roomId>"   TTL 7200
//   instance:<type>:<roomId>      SET  accountId           TTL 7200
//
// TTL 은 서버가 비정상 종료했을 때의 안전망일 뿐이다. 정상 경로에서는 방을
// 나갈 때 지운다. 남아 있는 낡은 계정 번호는 해가 없다 — 전송은 지금 접속 중인
// 세션만 골라 가고(Room::sendToAccounts), 없는 계정은 조용히 건너뛴다.

#include <cstdint>
#include <string>
#include <vector>

namespace heaven::net {
class RedisClient;
}

namespace heaven::instancechat {

// 서버가 죽어 정리를 못 했을 때 남는 찌꺼기의 수명.
inline constexpr int kPresenceTtlSeconds = 7200;

class InstancePresence {
public:
    explicit InstancePresence(net::RedisClient& redis) : redis_(redis) {}

    // 방에 들어왔다고 알린다.
    void enter(std::uint64_t accountId, std::uint32_t instanceType, std::uint32_t roomId);

    // 방에서 나갔다고 알린다. 어느 방이었는지는 저장소가 기억한다.
    void leave(std::uint64_t accountId);

    // 이 계정과 같은 방에 있는 계정 전부 (본인 포함). 인스턴스 밖이면 빈 목록.
    std::vector<std::uint64_t> roommates(std::uint64_t accountId);

private:
    net::RedisClient& redis_;
};

}  // namespace heaven::instancechat
