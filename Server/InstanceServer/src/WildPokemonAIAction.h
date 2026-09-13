#pragma once

#include "AiTypes.h"
#include "Map.h"
#include "Path.h"
#include <random>
#include <vector>

namespace heaven::instance {

// 이동 상태와 전투 추격의 경로 실행기. 배회는 경로 탐색을 사용하지 않는다.
class WildPokemonAIMovement {
public:
    bool begin(const Map* map, nav::Vec3 position, nav::Vec3 goal,
               float acceptanceRadius, float restAfterArrive);
    WildIntent follow(const Map* map, nav::Vec3 position);
    void stop();
    void blocked();
    void advanceTime(float dt);

    bool moving = false;
    float restRemaining = 0.f;
    float replanRemaining = 0.f;
    nav::Vec3 requestedGoal;

private:
    std::vector<nav::Vec3> path_;
    std::size_t pathIndex_ = 0;
    nav::Vec3 goal_;
    float acceptanceRadius_ = 80.f;
    float restAfterArrive_ = 2.f;
};

struct WildPokemonAIActionMemory {
    WildPokemonAIMovement movement;
    std::uint64_t targetId = 0;
    float attackCooldown = 0.f;
    nav::Vec3 wanderDirection;
    float wanderMoveRemaining = 0.f;
    float wanderRestSeconds = 2.f;

    void advanceTime(float dt);
    void finishWander();
};

struct WildPokemonAIActionContext {
    nav::Vec3 position;
    const Map* map = nullptr;
    WildArea area;
    const std::vector<ObservedPlayer>& players;
    std::uint32_t mapId = 0;
};

float boundedAIValue(float value, float fallback, float minimum, float maximum);
const ObservedPlayer* findActionTarget(const WildPokemonAIActionContext& context,
                                      std::uint64_t targetId);

} // namespace heaven::instance
