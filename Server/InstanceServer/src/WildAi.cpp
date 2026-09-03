#include "WildAi.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <utility>

#include "WildBt.h"

namespace heaven::instance {

namespace {

constexpr float kDefaultWanderRadius = 1500.f;
constexpr float kMinWanderRadius = 100.f;
constexpr float kMaxWanderRadius = 3000.f;
constexpr float kDefaultArriveRadius = 80.f;
constexpr float kMinAcceptanceRadius = 30.f;
constexpr float kMaxAcceptanceRadius = 600.f;
constexpr float kMinRestSeconds = 0.1f;
constexpr float kMaxRestSeconds = 8.f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kAggroRadius = 900.f;
constexpr float kLoseTargetRadius = 1800.f;
constexpr float kChaseRepathDistance = 180.f;
constexpr float kActionRetrySeconds = 0.5f;
constexpr float kMinReconsiderSeconds = 0.1f;
constexpr float kMaxReconsiderSeconds = 2.f;
constexpr int kWanderGoalAttempts = 8;

float clampAreaX(float value, const WildArea& area) {
    return std::clamp(value, area.centerX - area.halfExtent, area.centerX + area.halfExtent);
}

float clampAreaY(float value, const WildArea& area) {
    return std::clamp(value, area.centerY - area.halfExtent, area.centerY + area.halfExtent);
}

float clampFinite(float value, float fallback, float minValue, float maxValue) {
    return std::isfinite(value) ? std::clamp(value, minValue, maxValue) : fallback;
}

const ObservedPlayer* findPlayerById(const std::vector<ObservedPlayer>& players,
                                     std::uint64_t entityId, std::uint32_t mapId) {
    if (entityId == 0) {
        return nullptr;
    }
    for (const ObservedPlayer& player : players) {
        if (player.entityId == entityId && player.mapId == mapId) {
            return &player;
        }
    }
    return nullptr;
}

const ObservedPlayer* findNearestPlayer(float x, float y, std::uint32_t mapId,
                                        const std::vector<ObservedPlayer>& players,
                                        float radius) {
    const float radiusSquared = radius * radius;
    const ObservedPlayer* nearest = nullptr;
    float nearestDistanceSquared = radiusSquared;
    for (const ObservedPlayer& player : players) {
        if (player.mapId != mapId) {
            continue;
        }
        const float dx = player.x - x;
        const float dy = player.y - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 <= nearestDistanceSquared) {
            nearest = &player;
            nearestDistanceSquared = d2;
        }
    }
    return nearest;
}

bool isPlayerInRange(float x, float y, const ObservedPlayer& player, float radius) {
    const float dx = player.x - x;
    const float dy = player.y - y;
    return dx * dx + dy * dy <= radius * radius;
}

}  // namespace

WildAi::WildAi(std::unique_ptr<WildBt> behavior)
    : behavior_(std::move(behavior)), rng_(std::random_device{}()) {
    if (behavior_ == nullptr) {
        throw std::invalid_argument("wild behavior tree is required");
    }
}

WildAi::~WildAi() = default;

WildIntent WildAi::decide(std::uint64_t entityId, std::uint16_t species, std::uint32_t mapId,
                          float x, float y, float dt,
                          const std::vector<ObservedPlayer>& players) {
    WildBrain& brain = brains_[entityId];
    const float safeDt = std::max(dt, 0.f);
    brain.replanRemainingSeconds = std::max(0.f, brain.replanRemainingSeconds - safeDt);

    const ObservedPlayer* aggroPlayer =
        findNearestPlayer(x, y, mapId, players, kAggroRadius);
    if (brain.action != RunningAction::Chase && aggroPlayer != nullptr) {
        requestDecision(brain);
    }

    if (brain.action == RunningAction::Chase && brain.targetId != 0) {
        const ObservedPlayer* target = findPlayerById(players, brain.targetId, mapId);
        if (target == nullptr || !isPlayerInRange(x, y, *target, kLoseTargetRadius)) {
            brain.targetId = 0;
            brain.action = RunningAction::None;
            requestDecision(brain);
        } else if (brain.phase == WildPhase::Moving &&
                   distanceSquared(target->x, target->y, brain.lastTargetX,
                                   brain.lastTargetY) >
                       kChaseRepathDistance * kChaseRepathDistance &&
                   brain.replanRemainingSeconds <= 0.f) {
            requestDecision(brain);
        }
    }

    if (brain.phase == WildPhase::Moving) {
        WildIntent intent = followPath(x, y, brain);
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
        brain.phase = WildPhase::NeedDecision;
    }

    return chooseNextAction(entityId, species, mapId, x, y, brain, players);
}

void WildAi::notifyMoveBlocked(std::uint64_t entityId) {
    const auto it = brains_.find(entityId);
    if (it == brains_.end()) {
        return;
    }

    // 막힌 목표를 계속 붙잡으면 매 틱 같은 이동을 시도한다.
    // 짧게 쉰 뒤 Lua BT 에 새 action 을 묻는다.
    it->second.path.clear();
    it->second.pathIndex = 0;
    beginRest(it->second, 0.25f);
}

void WildAi::seed(unsigned value) {
    if (value == 0) {
        return;
    }
    rng_.seed(value);
    behavior_->seed(value);
}

WildIntent WildAi::chooseNextAction(std::uint64_t entityId, std::uint16_t species,
                                    std::uint32_t mapId, float x, float y,
                                    WildBrain& brain,
                                    const std::vector<ObservedPlayer>& players) {
    const ObservedPlayer* currentTarget = findPlayerById(players, brain.targetId, mapId);
    if (currentTarget != nullptr && !isPlayerInRange(x, y, *currentTarget, kLoseTargetRadius)) {
        currentTarget = nullptr;
    }
    const ObservedPlayer* nearestPlayer =
        findNearestPlayer(x, y, mapId, players, kLoseTargetRadius);

    WildBtContext context;
    context.entityId = entityId;
    context.species = species;
    context.mapId = mapId;
    context.x = x;
    context.y = y;
    context.currentTarget = currentTarget;
    context.nearestPlayer = nearestPlayer;
    context.aggroRadius = kAggroRadius;
    context.loseTargetRadius = kLoseTargetRadius;

    const WildDecision decision = behavior_->decide(context);
    if (!decision.valid) {
        beginRest(brain, kActionRetrySeconds);
        return {};
    }

    if (decision.action == WildDecision::Action::Wander) {
        brain.action = RunningAction::Wander;
        brain.targetId = 0;
        brain.replanRemainingSeconds = 0.f;

        for (int attempt = 0; attempt < kWanderGoalAttempts; ++attempt) {
            const MoveAction action = makeWanderAction(x, y, decision);
            if (action.valid && beginMove(x, y, action, brain)) {
                brain.phase = WildPhase::Moving;
                WildIntent intent = followPath(x, y, brain);
                if (!intent.moving) {
                    beginRest(brain, brain.restAfterArriveSeconds);
                }
                return intent;
            }
        }

        beginRest(brain, kActionRetrySeconds);
        return {};
    }

    const ObservedPlayer* actionTarget = findPlayerById(players, decision.targetId, mapId);
    const bool keepingCurrentTarget =
        brain.action == RunningAction::Chase && decision.targetId == brain.targetId;
    const float allowedRadius = keepingCurrentTarget ? kLoseTargetRadius : kAggroRadius;
    if (actionTarget == nullptr || !isPlayerInRange(x, y, *actionTarget, allowedRadius)) {
        beginRest(brain, kActionRetrySeconds);
        return {};
    }

    const MoveAction action = makeChaseAction(decision, *actionTarget);
    if (!action.valid || !beginMove(x, y, action, brain)) {
        beginRest(brain, kActionRetrySeconds);
        return {};
    }

    brain.action = RunningAction::Chase;
    brain.targetId = actionTarget->entityId;
    brain.lastTargetX = actionTarget->x;
    brain.lastTargetY = actionTarget->y;
    brain.replanRemainingSeconds = action.restAfterArriveSeconds;
    brain.phase = WildPhase::Moving;

    WildIntent intent = followPath(x, y, brain);
    if (!intent.moving) {
        beginRest(brain, brain.restAfterArriveSeconds);
    }
    return intent;
}

bool WildAi::beginMove(float x, float y, const MoveAction& action, WildBrain& brain) {
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

WildIntent WildAi::followPath(float x, float y, WildBrain& brain) {
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

WildAi::MoveAction WildAi::makeWanderAction(float x, float y,
                                            const WildDecision& decision) {
    std::uniform_real_distribution<float> unit(0.f, 1.f);

    const float wanderRadius = clampFinite(decision.wanderRadius,
                                           kDefaultWanderRadius,
                                           kMinWanderRadius,
                                           kMaxWanderRadius);
    const float angle = unit(rng_) * kPi * 2.f;
    const float radius = unit(rng_) * wanderRadius;

    MoveAction action;
    action.targetX = clampAreaX(x + std::cos(angle) * radius, area_);
    action.targetY = clampAreaY(y + std::sin(angle) * radius, area_);
    action.acceptanceRadius = clampFinite(decision.acceptanceRadius,
                                          kDefaultArriveRadius,
                                          kMinAcceptanceRadius,
                                          kMaxAcceptanceRadius);
    action.restAfterArriveSeconds = clampFinite(decision.restSeconds,
                                                2.f,
                                                kMinRestSeconds,
                                                kMaxRestSeconds);
    action.valid =
        std::isfinite(action.targetX) &&
        std::isfinite(action.targetY) &&
        std::isfinite(action.acceptanceRadius) &&
        std::isfinite(action.restAfterArriveSeconds);
    return action;
}

WildAi::MoveAction WildAi::makeChaseAction(const WildDecision& decision,
                                           const ObservedPlayer& target) {
    MoveAction action;
    action.targetX = target.x;
    action.targetY = target.y;
    action.acceptanceRadius = clampFinite(decision.acceptanceRadius,
                                          140.f,
                                          kMinAcceptanceRadius,
                                          kMaxAcceptanceRadius);
    action.restAfterArriveSeconds = clampFinite(decision.reconsiderSeconds,
                                                kActionRetrySeconds,
                                                kMinReconsiderSeconds,
                                                kMaxReconsiderSeconds);
    action.valid = std::isfinite(action.targetX) && std::isfinite(action.targetY);
    return action;
}

void WildAi::requestDecision(WildBrain& brain) {
    brain.phase = WildPhase::NeedDecision;
    brain.restRemainingSeconds = 0.f;
    brain.path.clear();
    brain.pathIndex = 0;
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
