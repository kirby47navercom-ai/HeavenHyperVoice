#pragma once

#include <cstdint>

namespace heaven::instance {

struct ObservedPlayer {
    std::uint64_t entityId = 0;
    std::uint32_t mapId = 0;
    float x = 0.f;
    float y = 0.f;
};

struct WildDecision {
    enum class Action : std::uint8_t {
        Wander,
        Chase
    };

    Action action = Action::Wander;
    std::uint64_t targetId = 0;
    float wanderRadius = 1500.f;
    float acceptanceRadius = 80.f;
    float restSeconds = 2.f;
    float reconsiderSeconds = 0.5f;
    bool valid = false;
};

}  // namespace heaven::instance
