#include "WildPokemonAIAction.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace heaven::instance {

namespace {

float horizontalDistanceSquared(nav::Vec3 a, nav::Vec3 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

} // namespace

float boundedAIValue(float value, float fallback, float minimum, float maximum) {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

const ObservedPlayer* findActionTarget(const WildPokemonAIActionContext& context,
                                      std::uint64_t targetId) {
    for (const ObservedPlayer& player : context.players) {
        if (player.entityId == targetId && player.mapId == context.mapId &&
            std::isfinite(player.x) && std::isfinite(player.y) && std::isfinite(player.z)) {
            return &player;
        }
    }

    return nullptr;
}

void WildPokemonAIMovement::advanceTime(float dt) {
    restRemaining = std::max(0.f, restRemaining - dt);
    replanRemaining = std::max(0.f, replanRemaining - dt);
}

void WildPokemonAIActionMemory::advanceTime(float dt) {
    movement.advanceTime(dt);
    attackCooldown = std::max(0.f, attackCooldown - dt);
    wanderMoveRemaining = std::max(0.f, wanderMoveRemaining - dt);
}

void WildPokemonAIActionMemory::finishWander() {
    movement.stop();
    wanderMoveRemaining = 0.f;
    movement.restRemaining = wanderRestSeconds;
}

void WildPokemonAIMovement::stop() {
    moving = false;
    path_.clear();
    pathIndex_ = 0;
}

void WildPokemonAIMovement::blocked() {
    stop();
    restRemaining = 0.25f;
}

bool WildPokemonAIMovement::begin(const Map* map, nav::Vec3 position, nav::Vec3 goal,
                                  float acceptanceRadius, float restAfterArrive) {
    stop();
    if (!map || !map->loaded()) {
        return false;
    }

    requestedGoal = goal;
    acceptanceRadius_ = acceptanceRadius;
    restAfterArrive_ = restAfterArrive;
    const auto& agent = map->agent();
    nav::Vec3 groundedGoal;

    if (!map->canStandAt(goal.x, goal.y, agent, &groundedGoal, goal.z) &&
        !map->nearestStandable(goal.x, goal.y, std::max(acceptanceRadius, agent.radius * 4.f),
                               agent, groundedGoal, goal.z)) {
        return false;
    }

    goal_ = groundedGoal;
    if (horizontalDistanceSquared(position, goal_) > acceptanceRadius_ * acceptanceRadius_) {
        PathResult path = Pathfinder{}.find(*map, position, goal_, agent);
        if (!path.found || path.points.empty()) {
            return false;
        }

        path_ = std::move(path.points);
    }

    moving = true;
    restRemaining = 0.f;
    return true;
}

WildIntent WildPokemonAIMovement::follow(const Map* map, nav::Vec3 position) {
    if (!moving || !map) {
        return {};
    }

    constexpr float WaypointRadiusSquared = 35.f * 35.f;
    while (pathIndex_ < path_.size() &&
           horizontalDistanceSquared(position, path_[pathIndex_]) <= WaypointRadiusSquared) {
        const auto next = pathIndex_ + 1 < path_.size() ? path_[pathIndex_ + 1] : goal_;
        if (map->blockedAlong(position, next, map->agent()) &&
            horizontalDistanceSquared(position, path_[pathIndex_]) > 0.25f) {
            break;
        }

        ++pathIndex_;
    }

    if (pathIndex_ >= path_.size() &&
        horizontalDistanceSquared(position, goal_) <= acceptanceRadius_ * acceptanceRadius_) {
        stop();
        restRemaining = restAfterArrive_;
        return {};
    }

    const auto target = pathIndex_ < path_.size() ? path_[pathIndex_] : goal_;
    WildIntent intent;
    intent.targetX = target.x;
    intent.targetY = target.y;
    intent.moving = true;
    return intent;
}

} // namespace heaven::instance
