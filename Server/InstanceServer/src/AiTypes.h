#pragma once

#include "WildPokemonAIFSM.h"
#include <cstdint>

namespace heaven::instance {

struct ObservedPlayer {
    std::uint64_t entityId = 0;
    std::uint32_t mapId = 0;
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

struct WildArea {
    float centerX = 0.f;
    float centerY = 0.f;
    float halfExtent = 4000.f;
};

struct WildIntent {
    float targetX = 0.f;
    float targetY = 0.f;
    float attackRange = 180.f;
    std::uint64_t attackTargetId = 0;
    bool moving = false;
    bool attacking = false;
};

// Lua는 전환 요청과 현재 상태에서 수행할 명령을 반환한다.
struct WildDecision {
    enum class Action : std::uint8_t {
        Continue,
        Wait,
        Wander,
        Chase,
        Attack
    };

    WildPokemonAIState nextState = WildPokemonAIState::Wander;
    Action action = Action::Wait;
    std::uint64_t targetId = 0;
    float moveSeconds = 2.f;
    float attackRange = 180.f;
    float acceptanceRadius = 80.f;
    float restSeconds = 2.f;
    float reconsiderSeconds = 0.5f;
    bool valid = false;
};

} // namespace heaven::instance
