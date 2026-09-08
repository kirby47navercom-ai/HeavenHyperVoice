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

float distanceSquared(const nav::Vec3& a, const nav::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

float distance2D(const nav::Vec3& a, const nav::Vec3& b) {
    return std::sqrt(distanceSquared(a, b));
}

float length2D(const nav::Vec3& value) {
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

nav::Vec3 standingSpot(const PartnerOwnerState& owner, float sideSign,
                       const PartnerFollowConfig& config) {
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

bool resolveStandable(const nav::Vec3& location, const Map* map,
                      const PartnerFollowConfig& config, nav::Vec3& out) {
    if (map == nullptr || !map->loaded()) {
        out = location;
        return true;
    }

    const nav::Agent& agent = map->agent();
    if (map->canStandAt(location.x, location.y, agent, &out)) {
        return true;
    }

    const float searchRadius = std::max(
        std::max(config.followForwardOffset, config.followSideOffset),
        agent.radius * 4.f);
    return map->nearestStandable(location.x, location.y, searchRadius, agent, out);
}

Spot resolveSpot(const PartnerOwnerState& owner, float sideSign, const Map* map,
                 const PartnerFollowConfig& config) {
    Spot spot;
    spot.sideSign = sideSign;
    spot.valid = resolveStandable(standingSpot(owner, sideSign, config), map, config,
                                  spot.location);
    return spot;
}

bool movementBlocked(const nav::Vec3& from, const nav::Vec3& to, const Map* map) {
    return map != nullptr && map->loaded() && map->blockedAlong(from, to, map->agent());
}

void clearPath(PartnerState& state) {
    state.path.clear();
    state.pathIndex = 0;
    state.pathGoal = {};
}

bool stopPartner(PartnerState& state) {
    const bool wasMoving = length2D(state.velocity) > kSmallDistance;
    state.velocity = {};
    state.movedThisTick = wasMoving;
    state.teleportedThisTick = false;
    clearPath(state);
    return wasMoving;
}

nav::Vec3 avoidOwner(const nav::Vec3& from, const nav::Vec3& to,
                     const PartnerOwnerState& owner, float sideSign,
                     const PartnerFollowConfig& config) {
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
        owner.location.x +
            right.x * (config.ownerAvoidRadius + config.followSideOffset * 0.5f) * sideSign,
        owner.location.y +
            right.y * (config.ownerAvoidRadius + config.followSideOffset * 0.5f) * sideSign,
        owner.location.z,
    };
}

bool pathNeedsRefresh(const PartnerState& state, const nav::Vec3& goal,
                      const PartnerFollowConfig& config) {
    return state.path.empty() ||
           state.pathIndex >= state.path.size() ||
           distance2D(state.pathGoal, goal) > config.repathDistance ||
           std::abs(state.pathGoal.z - goal.z) > config.arriveDistance;
}

bool refreshPath(PartnerState& state, const nav::Vec3& goal, const Map* map,
                 const PartnerFollowConfig& config) {
    if (map == nullptr || !map->loaded()) {
        clearPath(state);
        return true;
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

nav::Vec3 nextPathTarget(PartnerState& state, const nav::Vec3& goal,
                         const PartnerFollowConfig& config) {
    const float waypointRadius = std::max(config.waypointRadius, 1.f);
    while (state.pathIndex < state.path.size() &&
           distance2D(state.location, state.path[state.pathIndex]) <= waypointRadius) {
        ++state.pathIndex;
    }

    if (state.pathIndex < state.path.size()) {
        return state.path[state.pathIndex];
    }
    return goal;
}

}  // namespace

void PartnerFollower::reset(PartnerState& state) {
    state = {};
}

bool PartnerFollower::initialize(const PartnerOwnerState& owner, std::uint16_t species,
                                 PartnerState& state, const Map* map,
                                 const PartnerFollowConfig& config) {
    if (species == 0) {
        reset(state);
        return false;
    }

    const Spot right = resolveSpot(owner, 1.f, map, config);
    const Spot left = resolveSpot(owner, -1.f, map, config);
    const Spot* chosen = right.valid ? &right : (left.valid ? &left : nullptr);
    if (chosen == nullptr) {
        reset(state);
        return false;
    }

    state.initialized = true;
    state.location = chosen->location;
    state.velocity = {};
    state.facing = owner.facing;
    state.sideSign = chosen->sideSign;
    state.idleSeconds = 0.f;
    clearPath(state);
    state.movedThisTick = true;
    state.teleportedThisTick = true;
    return true;
}

bool PartnerFollower::update(float dt, const PartnerOwnerState& owner, std::uint16_t species,
                             PartnerState& state, const Map* map,
                             const PartnerFollowConfig& config) {
    if (species == 0) {
        reset(state);
        return false;
    }
    if (!state.initialized) {
        return initialize(owner, species, state, map, config);
    }

    state.movedThisTick = false;
    state.teleportedThisTick = false;

    const Spot kept = resolveSpot(owner, state.sideSign, map, config);
    const Spot other = resolveSpot(owner, -state.sideSign, map, config);
    const Spot* chosen = nullptr;
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

    if (chosen == nullptr) {
        return stopPartner(state);
    }

    state.sideSign = chosen->sideSign;
    const nav::Vec3 target = chosen->location;
    const float targetDistance = distance2D(state.location, target);
    if (targetDistance <= std::max(config.arriveDistance, 1.f)) {
        clearPath(state);
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
        state.location = target;
        state.velocity = {};
        if (std::sqrt(dx * dx + dy * dy) > kSmallDistance) {
            state.facing = yawOf(dx, dy);
        }
        state.movedThisTick = true;
        state.teleportedThisTick = true;
        clearPath(state);
        return true;
    }

    nav::Vec3 moveTarget = target;
    if (movementBlocked(state.location, target, map)) {
        if (!refreshPath(state, target, map, config)) {
            return stopPartner(state);
        }
        moveTarget = nextPathTarget(state, target, config);
    } else {
        clearPath(state);
    }

    const nav::Vec3 ownerAvoidTarget =
        avoidOwner(state.location, moveTarget, owner, state.sideSign, config);
    if (ownerAvoidTarget.x != moveTarget.x || ownerAvoidTarget.y != moveTarget.y) {
        nav::Vec3 resolved;
        if (resolveStandable(ownerAvoidTarget, map, config, resolved)) {
            moveTarget = resolved;
        }
    }

    if (movementBlocked(state.location, moveTarget, map)) {
        return stopPartner(state);
    }

    const float dx = moveTarget.x - state.location.x;
    const float dy = moveTarget.y - state.location.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance <= kSmallDistance) {
        return stopPartner(state);
    }

    const float ownerSpeed = length2D(owner.velocity);
    const float baseSpeed = std::max(ownerSpeed, pokemonMoveSpeed(species));
    const float chaseSpeed = std::min(
        baseSpeed + std::max(targetDistance - config.arriveDistance, 0.f) * config.catchUpGain,
        config.maxFollowSpeed);

    const float safeDt = std::max(dt, 1e-3f);
    const float step = chaseSpeed * safeDt;
    const float ratio = step >= distance ? 1.f : step / distance;
    nav::Vec3 next{
        state.location.x + dx * ratio,
        state.location.y + dy * ratio,
        state.location.z + (moveTarget.z - state.location.z) * ratio,
    };

    if (map != nullptr && map->loaded()) {
        nav::Vec3 grounded;
        if (!map->canStandAt(next.x, next.y, map->agent(), &grounded) ||
            map->blockedAlong(state.location, grounded, map->agent())) {
            return stopPartner(state);
        }
        next = grounded;
    }

    const nav::Vec3 previous = state.location;
    state.location = next;
    state.velocity = {
        (next.x - previous.x) / safeDt,
        (next.y - previous.y) / safeDt,
        (next.z - previous.z) / safeDt,
    };
    state.facing = yawOf(dx, dy);
    state.movedThisTick = true;
    return true;
}

}  // namespace heaven::fieldshared
