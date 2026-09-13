#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Map.h"

namespace heaven::fieldshared {

struct PartnerFollowConfig {
    float followForwardOffset = 150.f;
    float followSideOffset = 150.f;
    float arriveDistance = 45.f;
    float sideSwitchMargin = 80.f;
    float ownerAvoidRadius = 90.f;
    float faceOwnerDelay = 1.f;
    float catchUpGain = 2.f;
    float maxFollowSpeed = 1200.f;
    float teleportDistance = 900.f;
    float repathDistance = 80.f;
    float waypointRadius = 35.f;
    float blockedRecoverySeconds = 2.f;
    float pathRetrySeconds = 0.25f;
};

struct PartnerOwnerState {
    nav::Vec3 location;
    nav::Vec3 velocity;
    float facing = 0.f;
};

struct PartnerState {
    hhv::movement::State movement;
    float accumulator = 0.f;
    nav::Vec3 moveTarget;
    float moveSpeed = 0.f;
    bool wantsMove = false;
    bool initialized = false;
    nav::Vec3 location;
    nav::Vec3 velocity;
    float facing = 0.f;

    // +1 이 주인 기준 오른쪽, -1 이 왼쪽이다.
    float sideSign = 1.f;
    float idleSeconds = 0.f;
    float blockedSeconds = 0.f;
    float retrySeconds = 0.f;
    nav::Vec3 pathGoal;
    std::vector<nav::Vec3> path;
    std::size_t pathIndex = 0;

    bool movedThisTick = false;
    bool teleportedThisTick = false;
};

class PartnerFollower {
  public:
    static bool initialize(const PartnerOwnerState &owner, std::uint16_t species, PartnerState &state,
                           const Map *map, const PartnerFollowConfig &config = {});

    static bool update(float dt, const PartnerOwnerState &owner, std::uint16_t species, PartnerState &state,
                       const Map *map, const PartnerFollowConfig &config = {});

    static void reset(PartnerState &state);
};

} // namespace heaven::fieldshared
