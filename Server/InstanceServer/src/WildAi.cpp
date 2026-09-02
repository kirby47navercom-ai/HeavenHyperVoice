#include "WildAi.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

#include "FieldGeometry.h"

namespace heaven::instance {

namespace {

constexpr float kWanderRadius = 1500.f;
constexpr float kArriveRadius = 80.f;
constexpr float kRestMinSeconds = 1.5f;
constexpr float kRestMaxSeconds = 4.f;
constexpr float kPi = 3.14159265358979323846f;

// 배회 구역은 방마다 다르다 (맵 경계에서 나온다). 상수로 둘 수 없어 인자로 받는다.
float clampAreaX(float value, const WildArea& area) {
    return std::clamp(value, area.centerX - area.halfExtent, area.centerX + area.halfExtent);
}

float clampAreaY(float value, const WildArea& area) {
    return std::clamp(value, area.centerY - area.halfExtent, area.centerY + area.halfExtent);
}

}  // namespace

WildAi::WildAi() : rng_(std::random_device{}()) {}

WildIntent WildAi::decide(std::uint64_t entityId, std::uint16_t species, float x, float y,
                          float dt) {
    (void)species;

    WildBrain& brain = brains_[entityId];
    if (!brain.hasHome) {
        brain.homeX = x;
        brain.homeY = y;
        brain.hasHome = true;
    }

    switch (brain.mode) {
        case WildMode::Wander:
            return tickWander(x, y, dt, brain);
        case WildMode::Combat:
        case WildMode::Downed:
        default:
            return {};
    }
}

void WildAi::notifyMoveBlocked(std::uint64_t entityId) {
    const auto it = brains_.find(entityId);
    if (it == brains_.end()) {
        return;
    }

    // 막힌 목표를 계속 붙잡으면 매 틱 같은 이동을 시도한다.
    // 짧게 쉰 뒤 새 wander action 을 뽑는다.
    it->second.path.clear();
    it->second.pathIndex = 0;
    beginRest(it->second, 0.25f);
}

void WildAi::seed(unsigned value) {
    if (value == 0) {
        return;
    }
    rng_.seed(value);
}

WildIntent WildAi::tickWander(float x, float y, float dt, WildBrain& brain) {
    const float safeDt = std::max(dt, 0.f);

    if (brain.phase == WildPhase::Moving) {
        WildIntent intent = followWanderPath(x, y, brain);
        if (!intent.moving) {
            beginRest(brain, brain.restAfterArriveSeconds);
        }
        return intent;
    }

    if (brain.phase == WildPhase::Resting) {
        brain.restRemainingSeconds = std::max(0.f, brain.restRemainingSeconds - safeDt);
        if (brain.restRemainingSeconds > 0.f) {
            return {};
        }
        brain.phase = WildPhase::NeedAction;
    }

    const WanderActionResult action = makeWanderAction(x, y);
    if (!action.valid) {
        return {};
    }

    if (!beginWanderMove(x, y, action, brain)) {
        beginRest(brain, 0.75f);
        return {};
    }

    brain.phase = WildPhase::Moving;
    WildIntent intent = followWanderPath(x, y, brain);
    if (!intent.moving) {
        beginRest(brain, brain.restAfterArriveSeconds);
    }
    return intent;
}

bool WildAi::beginWanderMove(float x, float y, const WanderActionResult& action,
                             WildBrain& brain) {
    brain.targetX = action.targetX;
    brain.targetY = action.targetY;
    brain.acceptanceRadius = std::max(action.acceptanceRadius, 1.f);
    brain.restAfterArriveSeconds = std::max(action.restAfterArriveSeconds, 0.f);
    brain.path.clear();
    brain.pathIndex = 0;

    if (distanceSquared(x, y, brain.targetX, brain.targetY) <=
        brain.acceptanceRadius * brain.acceptanceRadius) {
        return true;
    }

    if (map_ != nullptr && map_->loaded()) {
        nav::Vec3 goal{brain.targetX, brain.targetY, agent_.halfHeight};
        nav::Vec3 groundedGoal;
        if (map_->canStandAt(goal.x, goal.y, agent_, &groundedGoal)) {
            goal = groundedGoal;
        } else if (map_->nearestStandable(
                       goal.x,
                       goal.y,
                       std::max(brain.acceptanceRadius, agent_.radius * 4.f),
                       agent_,
                       groundedGoal)) {
            goal = groundedGoal;
        } else {
            return false;
        }

        PathResult path = pathfinder_.find(
            *map_,
            nav::Vec3{x, y, agent_.halfHeight},
            goal,
            agent_);
        if (!path.found || path.points.empty()) {
            return false;
        }

        brain.targetX = goal.x;
        brain.targetY = goal.y;
        brain.path = std::move(path.points);
    }

    return true;
}

WildIntent WildAi::followWanderPath(float x, float y, WildBrain& brain) {
    constexpr float kWaypointRadius = 35.f;
    const float waypointRadiusSquared = kWaypointRadius * kWaypointRadius;

    while (brain.pathIndex < brain.path.size() &&
           distanceSquared(x, y, brain.path[brain.pathIndex].x, brain.path[brain.pathIndex].y) <=
               waypointRadiusSquared) {
        ++brain.pathIndex;
    }

    if (brain.pathIndex < brain.path.size()) {
        const nav::Vec3& waypoint = brain.path[brain.pathIndex];
        return WildIntent{waypoint.x, waypoint.y, brain.acceptanceRadius, true};
    }

    if (distanceSquared(x, y, brain.targetX, brain.targetY) <=
        brain.acceptanceRadius * brain.acceptanceRadius) {
        return {};
    }

    return WildIntent{brain.targetX, brain.targetY, brain.acceptanceRadius, true};
}

WildAi::WanderActionResult WildAi::makeWanderAction(float x, float y) {
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    std::uniform_real_distribution<float> rest(kRestMinSeconds, kRestMaxSeconds);

    const float angle = unit(rng_) * kPi * 2.f;
    const float radius = unit(rng_) * kWanderRadius;

    WanderActionResult action;
    action.targetX = clampAreaX(x + std::cos(angle) * radius, area_);
    action.targetY = clampAreaY(y + std::sin(angle) * radius, area_);
    action.acceptanceRadius = kArriveRadius;
    action.restAfterArriveSeconds = rest(rng_);
    action.valid =
        std::isfinite(action.targetX) &&
        std::isfinite(action.targetY) &&
        std::isfinite(action.acceptanceRadius) &&
        std::isfinite(action.restAfterArriveSeconds);
    return action;
}

void WildAi::beginRest(WildBrain& brain, float seconds) {
    brain.phase = WildPhase::Resting;
    brain.restRemainingSeconds = std::max(seconds, 0.f);
}

float WildAi::distanceSquared(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

}  // namespace heaven::instance
