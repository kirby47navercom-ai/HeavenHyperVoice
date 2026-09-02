#pragma once

// 인스턴스 방의 생성·배정·회수.
//
// 방 하나 = World 하나다. 같은 종류(type)라도 정원이 차면 새 방을 만들어
// 나눠 담는다. 방끼리는 World 가 아예 달라서 서로의 엔티티를 볼 수 없고,
// 시야 계산도 섹터 격자도 방 안에서만 돈다 — 접속자가 몰려도 한 방의 비용은
// 정원으로 묶인다.
//
// 틱은 방마다 스레드를 띄우지 않는다. 스레드 N 개가 방을 `id % N` 으로 나눠
// 맡는다. 한 방은 언제나 같은 스레드가 돌리므로 WildAi 의 Lua VM 이 두
// 스레드에서 동시에 불릴 일이 없다.

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "MapCollision.h"
#include "WildAi.h"
#include "World.h"

namespace heaven::instance {

struct RoomSettings {
    // 방 하나의 정원. 넘으면 같은 종류의 다음 방으로 간다.
    int capacity = 20;

    // 종류당 방 개수 상한. 0 이면 무제한.
    // 무제한이라도 접속 수 자체가 상한 역할을 한다 (한 세션이 한 자리).
    int maxRoomsPerType = 0;

    // 방을 만들 때 뿌릴 야생 마릿수.
    int wildPerRoom = 12;

    // 빈 방을 이만큼 두었다가 정리한다. 0 이면 비는 즉시 없앤다.
    // 잠깐 나갔다 오는 사람이 매번 새 방을 만들지 않게 여유를 준다.
    std::chrono::seconds emptyLinger{60};

    // Lua 행동 스크립트. 방마다 자기 VM 을 하나씩 만든다.
    std::string wildScript;

    // 0 이면 매번 다르게. 지정하면 방 번호와 섞어 방마다 다른, 그러나
    // 재현 가능한 배치를 만든다.
    unsigned wildSeed = 0;
};

// 방 하나. World 와 WildAi 를 함께 소유한다.
//
// 이 객체의 주소는 만들어진 뒤 바뀌지 않는다 (unique_ptr 로 들고 있다).
// 세션이 Room* 를 붙잡고 있기 때문에 필요한 성질이다.
struct Room {
    std::uint32_t id = 0;
    std::uint32_t type = 0;

    World world;
    std::unique_ptr<WildAi> ai;  // 스크립트를 못 읽으면 비어 있다 (야생 없는 방)

    // 아래 둘은 RoomManager::mutex_ 를 쥔 채로만 만진다.
    int players = 0;
    std::chrono::steady_clock::time_point emptySince;
};

// 종류 하나의 설정. main 이 소유하고 서버보다 오래 산다.
//
// 종류마다 맵이 다르다 (1 번은 들판, 나중에 동굴 등이 붙는다). 맵이 없는
// 종류는 collision 이 nullptr 이고 지형 검사를 하지 않는다.
struct InstanceType {
    const MapCollision* collision = nullptr;

    // 여기 나올 야생 종족. **서버 내부 번호**다 (맵 파일은 도감번호로 적고
    // 읽을 때 바꾼다). 비어 있으면 종족 표 전체에서 고른다 — 전설까지 나오므로
    // 맵마다 적어 주는 편이 낫다.
    std::vector<std::uint16_t> wildSpecies;
};

class RoomManager {
public:
    // types 가 곧 받아줄 종류 목록이다. 여기 없는 번호는 거절한다 —
    // 클라이언트가 보낸 번호를 그대로 믿고 방을 만들면, 아무 숫자나 밀어 넣는
    // 것만으로 방을 무한히 만들어 메모리를 채울 수 있다.
    RoomManager(RoomSettings settings, std::map<std::uint32_t, InstanceType> types);
    ~RoomManager();

    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;

    bool isKnownType(std::uint32_t type) const;

    // 자리가 있는 방을 찾고, 없으면 새로 만든다. 종류를 모르거나 방 상한에
    // 걸리면 nullptr. 성공하면 그 방의 인원을 미리 한 자리 잡아 둔다 —
    // 배정과 증가를 나누면 그 틈에 정원을 넘겨 들어간다.
    Room* join(std::uint32_t type);

    // join 이 잡아 둔 자리를 돌려준다. 마지막 한 명이면 회수 대기로 넘어간다.
    void leave(Room* room);

    // shard 번째 스레드가 맡은 방들을 한 틱 전진시킨다.
    void tickShard(unsigned shard, unsigned shardCount, float dt);

    // 비어 있는 시간이 emptyLinger 를 넘긴 방을 없앤다. 틱 스레드 밖에서
    // 부를 것 — 방 객체를 지우므로 그 방을 돌리는 중이면 안 된다.
    void reapEmpty();

    std::size_t roomCount() const;
    std::size_t playerCount() const;

    // 로그용. "type=1 rooms=3 players=27" 같은 한 줄.
    std::string describe() const;

private:
    // mutex_ 를 쥔 채로 부른다.
    Room* createRoomLocked(std::uint32_t type);

    RoomSettings settings_;
    std::map<std::uint32_t, InstanceType> types_;

    // 목록과 인원 수를 지킨다. 방 **내용**(World) 은 방 자신의 락이 지킨다.
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<Room>> rooms_;
    std::uint32_t nextRoomId_ = 1;

    // 방의 **수명**을 지킨다. 틱은 공유 잠금으로 여럿이 동시에 돌고, 회수는
    // 배타 잠금으로 혼자 돈다. 이게 없으면 한 스레드가 돌리는 중인 방을
    // 다른 스레드가 지운다.
    //
    // rooms_ 벡터가 자라도 Room 주소는 안 바뀐다 (unique_ptr 라 옮겨지는 것은
    // 포인터뿐이다). 그래서 틱 스레드는 목록 사본만 뜨면 되고, 지켜야 할 것은
    // 회수뿐이다.
    mutable std::shared_mutex lifetimeMutex_;
};

}  // namespace heaven::instance
