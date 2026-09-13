#include "PartnerFollower.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Path.h"
#include "PokemonMovement.h"

namespace heaven::fieldshared {

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kSmallDistance = 1e-3f;
constexpr float kFacingEpsilon = 0.5f;

struct Spot {
    bool valid = false;
    nav::Vec3 location;
    float sideSign = 1.f;
};

float distanceSquared(const nav::Vec3 &a, const nav::Vec3 &b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

float distance2D(const nav::Vec3 &a, const nav::Vec3 &b) {
    return std::sqrt(distanceSquared(a, b));
}

float length2D(const nav::Vec3 &value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

float yawOf(float x, float y) {
    return std::atan2(y, x) * 180.f / kPi;
}

nav::Vec3 forwardOf(float yaw) {
    const float radians = yaw * kPi / 180.f;
    return {std::cos(radians), std::sin(radians), 0.f};
}

nav::Vec3 rightOf(float yaw) {
    const nav::Vec3 forward = forwardOf(yaw);
    return {-forward.y, forward.x, 0.f};
}

nav::Vec3 standingSpot(const PartnerOwnerState &owner, float sideSign, const PartnerFollowConfig &config) {
    const nav::Vec3 forward = forwardOf(owner.facing);
    const nav::Vec3 right = rightOf(owner.facing);
    return {
        owner.location.x + forward.x * config.followForwardOffset +
            right.x * config.followSideOffset * sideSign,
        owner.location.y + forward.y * config.followForwardOffset +
            right.y * config.followSideOffset * sideSign,
        owner.location.z,
    };
}

bool resolveStandable(const nav::Vec3 &location, const Map *map, const PartnerFollowConfig &config,
                      nav::Vec3 &out) {
    if (map == nullptr || !map->loaded()) {
        return false;
    }

    const nav::Agent &agent = map->agent();
    if (map->canStandAt(location.x, location.y, agent, &out, location.z)) {
        return true;
    }

    const float searchRadius =
        std::max(std::max(config.followForwardOffset, config.followSideOffset), agent.radius * 4.f);
    return map->nearestStandable(location.x, location.y, searchRadius, agent, out, location.z);
}

Spot resolveSpot(const PartnerOwnerState &owner, float sideSign, const Map *map,
                 const PartnerFollowConfig &config) {
    Spot spot;
    spot.sideSign = sideSign;
    spot.valid = resolveStandable(standingSpot(owner, sideSign, config), map, config, spot.location);
    return spot;
}

bool movementBlocked(const nav::Vec3 &from, const nav::Vec3 &to, const Map *map) {
    return map != nullptr && map->loaded() && map->blockedAlong(from, to, map->agent());
}

void clearPath(PartnerState &state) {
    state.path.clear();
    state.pathIndex = 0;
    state.pathGoal = {};
}

bool stopPartner(PartnerState &state) {
    const bool wasMoving = length2D(state.velocity) > kSmallDistance;
    state.velocity = {};
    state.movedThisTick = wasMoving;
    state.teleportedThisTick = false;
    clearPath(state);
    return wasMoving;
}

nav::Vec3 avoidOwner(const nav::Vec3 &from, const nav::Vec3 &to, const PartnerOwnerState &owner,
                     float sideSign, const PartnerFollowConfig &config) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= kSmallDistance) {
        return to;
    }

    const nav::Vec3 direction{dx / length, dy / length, 0.f};
    const float ownerDx = owner.location.x - from.x;
    const float ownerDy = owner.location.y - from.y;
    const float along = ownerDx * direction.x + ownerDy * direction.y;
    if (along <= 0.f || along >= length) {
        return to;
    }

    const float closestX = ownerDx - direction.x * along;
    const float closestY = ownerDy - direction.y * along;
    if (std::sqrt(closestX * closestX + closestY * closestY) >= config.ownerAvoidRadius) {
        return to;
    }

    const nav::Vec3 right = rightOf(owner.facing);
    return {
        owner.location.x + right.x * (config.ownerAvoidRadius + config.followSideOffset * 0.5f) * sideSign,
        owner.location.y + right.y * (config.ownerAvoidRadius + config.followSideOffset * 0.5f) * sideSign,
        owner.location.z,
    };
}

bool pathNeedsRefresh(const PartnerState &state, const nav::Vec3 &goal, const PartnerFollowConfig &config) {
    return state.path.empty() || state.pathIndex >= state.path.size() ||
           distance2D(state.pathGoal, goal) > config.repathDistance ||
           std::abs(state.pathGoal.z - goal.z) > config.arriveDistance;
}

bool refreshPath(PartnerState &state, const nav::Vec3 &goal, const Map *map,
                 const PartnerFollowConfig &config) {
    if (map == nullptr || !map->loaded()) {
        clearPath(state);
        return false;
    }

    if (!pathNeedsRefresh(state, goal, config)) {
        return true;
    }

    Pathfinder pathfinder;
    PathResult path = pathfinder.find(*map, state.location, goal, map->agent());
    if (!path.found || path.points.empty()) {
        clearPath(state);
        return false;
    }

    state.path = std::move(path.points);
    state.pathIndex = 0;
    state.pathGoal = goal;
    return true;
}

nav::Vec3 nextPathTarget(PartnerState &state, const nav::Vec3 &goal, const PartnerFollowConfig &config,
                         const Map *map) {
    const float waypointRadius = std::max(config.waypointRadius, 1.f);
    while (state.pathIndex < state.path.size() &&
           distance2D(state.location, state.path[state.pathIndex]) <= waypointRadius) {
        const nav::Vec3 next =
            state.pathIndex + 1 < state.path.size() ? state.path[state.pathIndex + 1] : goal;
        if (distance2D(state.location, state.path[state.pathIndex]) > 0.5f &&
            movementBlocked(state.location, next, map)) {
            break;
        }
        ++state.pathIndex;
    }

    if (state.pathIndex < state.path.size()) {
        return state.path[state.pathIndex];
    }
    return goal;
}

bool selectMoveTarget(PartnerState &state, const nav::Vec3 &goal, const Map *map,
                      const PartnerFollowConfig &config, nav::Vec3 &out) {
    if (!movementBlocked(state.location, goal, map)) {
        clearPath(state);
        out = goal;
        return true;
    }
    if (!refreshPath(state, goal, map, config)) {
        return false;
    }
    out = nextPathTarget(state, goal, config, map);
    return !movementBlocked(state.location, out, map);
}

bool teleportPartner(PartnerState &state, const nav::Vec3 &target, float facing) {
    state.location = target;
    state.velocity = {};
    state.facing = facing;
    state.blockedSeconds = 0.f;
    state.retrySeconds = 0.f;
    state.movedThisTick = true;
    state.teleportedThisTick = true;
    clearPath(state);
    return true;
}

} // namespace

void PartnerFollower::reset(PartnerState &state) {
    state = {};
}

bool PartnerFollower::initialize(const PartnerOwnerState &owner, std::uint16_t species, PartnerState &state,
                                 const Map *map, const PartnerFollowConfig &config) {
    if (species == 0) {
        reset(state);
        return false;
    }

    const Spot right = resolveSpot(owner, 1.f, map, config);
    const Spot left = resolveSpot(owner, -1.f, map, config);
    Spot fallback;
    const Spot *chosen = right.valid ? &right : (left.valid ? &left : nullptr);
    if (chosen == nullptr) {
        fallback.valid = resolveStandable(owner.location, map, config, fallback.location);
        if (!fallback.valid) {
            reset(state);
            return false;
        }
        chosen = &fallback;
    }

    if (!map || !map->loaded()) {
        return false;
    }
    state.movement = {};
    state.movement.position = map->toCore(chosen->location);
    state.movement.facing = owner.facing;
    state.accumulator = 0.f;
    state.initialized = true;
    state.location = chosen->location;
    state.velocity = {};
    state.facing = owner.facing;
    state.sideSign = chosen->sideSign;
    state.idleSeconds = 0.f;
    state.blockedSeconds = 0.f;
    state.retrySeconds = 0.f;
    clearPath(state);
    state.movedThisTick = true;
    state.teleportedThisTick = true;
    return true;
}

static bool chooseMovement(float dt, const PartnerOwnerState &owner, std::uint16_t species,
                           PartnerState &state, const Map *map, const PartnerFollowConfig &config) {
    if (species == 0) {
        PartnerFollower::reset(state);
        return false;
    }
    if (!state.initialized) {
        return PartnerFollower::initialize(owner, species, state, map, config);
    }

    state.movedThisTick = false;
    state.teleportedThisTick = false;

    const Spot kept = resolveSpot(owner, state.sideSign, map, config);
    const Spot other = resolveSpot(owner, -state.sideSign, map, config);
    const Spot *chosen = nullptr;
    if (kept.valid && other.valid) {
        chosen = &kept;
        if (distance2D(state.location, other.location) + config.sideSwitchMargin <
            distance2D(state.location, kept.location)) {
            chosen = &other;
        }
    } else if (kept.valid) {
        chosen = &kept;
    } else if (other.valid) {
        chosen = &other;
    }

    Spot fallback;
    if (chosen == nullptr) {
        fallback.sideSign = state.sideSign;
        fallback.valid = resolveStandable(owner.location, map, config, fallback.location);
        if (!fallback.valid) {
            return stopPartner(state);
        }
        chosen = &fallback;
    }

    state.sideSign = chosen->sideSign;
    nav::Vec3 target = chosen->location;
    float targetDistance = distance2D(state.location, target);
    if (targetDistance <= std::max(config.arriveDistance, 1.f)) {
        clearPath(state);
        state.blockedSeconds = 0.f;
        state.retrySeconds = 0.f;
        bool changed = false;
        if (length2D(state.velocity) > kSmallDistance) {
            state.velocity = {};
            changed = true;
        }

        state.idleSeconds += std::max(dt, 0.f);
        if (state.idleSeconds >= config.faceOwnerDelay) {
            const float dx = owner.location.x - state.location.x;
            const float dy = owner.location.y - state.location.y;
            if (std::sqrt(dx * dx + dy * dy) > kSmallDistance) {
                const float nextFacing = yawOf(dx, dy);
                if (std::abs(nextFacing - state.facing) > kFacingEpsilon) {
                    state.facing = nextFacing;
                    changed = true;
                }
            }
        }

        state.movedThisTick = changed;
        return changed;
    }

    state.idleSeconds = 0.f;

    if (targetDistance >= config.teleportDistance) {
        const float dx = target.x - state.location.x;
        const float dy = target.y - state.location.y;
        return teleportPartner(state, target, yawOf(dx, dy));
    }

    const auto blocked = [&] {
        state.blockedSeconds += std::max(dt, 0.f);
        if (state.blockedSeconds >= config.blockedRecoverySeconds) {
            return teleportPartner(state, target, owner.facing);
        }
        return stopPartner(state);
    };
    state.retrySeconds = std::max(0.f, state.retrySeconds - std::max(dt, 0.f));
    if (state.retrySeconds > 0.f) {
        return blocked();
    }
    nav::Vec3 moveTarget = target;
    if (!selectMoveTarget(state, target, map, config, moveTarget)) {
        const Spot &alternative = chosen == &kept ? other : kept;
        if (alternative.valid && selectMoveTarget(state, alternative.location, map, config, moveTarget)) {
            state.sideSign = alternative.sideSign;
            target = alternative.location;
            targetDistance = distance2D(state.location, target);
        } else {
            state.retrySeconds = config.pathRetrySeconds;
            return blocked();
        }
    }

    const nav::Vec3 ownerAvoidTarget = avoidOwner(state.location, moveTarget, owner, state.sideSign, config);
    if (ownerAvoidTarget.x != moveTarget.x || ownerAvoidTarget.y != moveTarget.y) {
        nav::Vec3 resolved;
        if (resolveStandable(ownerAvoidTarget, map, config, resolved) &&
            !movementBlocked(state.location, resolved, map)) {
            moveTarget = resolved;
        }
    }

    if (movementBlocked(state.location, moveTarget, map)) {
        state.retrySeconds = config.pathRetrySeconds;
        return blocked();
    }

    const float dx = moveTarget.x - state.location.x;
    const float dy = moveTarget.y - state.location.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance <= kSmallDistance) {
        return blocked();
    }

    const float ownerSpeed = length2D(owner.velocity);
    const float baseSpeed = std::max(ownerSpeed, pokemonMoveSpeed(species));
    const float chaseSpeed =
        std::min(baseSpeed + std::max(targetDistance - config.arriveDistance, 0.f) * config.catchUpGain,
                 config.maxFollowSpeed);

    state.moveTarget = moveTarget;
    state.moveSpeed = chaseSpeed;
    state.wantsMove = true;
    return true;
}

bool PartnerFollower::update(float dt, const PartnerOwnerState &owner, std::uint16_t species,
                             PartnerState &state, const Map *map, const PartnerFollowConfig &config) {
    if (!map || !map->loaded()) {
        reset(state);
        return false;
    }

    const auto previous = state.location;
    const auto previousVelocity = state.velocity;
    state.wantsMove = false;
    const bool changed = chooseMovement(dt, owner, species, state, map, config);
    if (!state.initialized) {
        return changed;
    }

    if (state.teleportedThisTick) {
        state.movement = {};
        state.movement.position = map->toCore(state.location);
        state.movement.facing = state.facing;
        state.accumulator = 0.f;
    }

    if (!state.wantsMove) {
        state.movement.facing = state.facing;
    }
    map->advance(state.movement, state.accumulator, dt, state.moveTarget, state.wantsMove, state.moveSpeed);
    state.location = map->toServer(state.movement.position);
    state.velocity = state.movement.velocity;
    state.facing = state.movement.facing;
    state.movedThisTick = changed || hhv::movement::length(state.location - previous) > .001f ||
                          hhv::movement::length(state.velocity - previousVelocity) > .001f;
    return state.movedThisTick;
}

} // namespace heaven::fieldshared
