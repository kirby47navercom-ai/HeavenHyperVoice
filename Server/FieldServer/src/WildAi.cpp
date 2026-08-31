#include "WildAi.h"

#include <spdlog/spdlog.h>
#include <sol/sol.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace heaven::field {

WildAi::WildAi(const std::string& scriptPath) : lua_(std::make_unique<sol::state>()) {
    lua_->open_libraries(sol::lib::base, sol::lib::math, sol::lib::table);

    const sol::protected_function_result loaded = lua_->safe_script_file(
        scriptPath, sol::script_pass_on_error);
    if (!loaded.valid()) {
        const sol::error err = loaded;
        throw std::runtime_error("cannot load wild AI script " + scriptPath + ": " + err.what());
    }

    const sol::object actionRoot = lua_->get<sol::object>("wild_actions");
    if (!actionRoot.is<sol::table>()) {
        throw std::runtime_error(scriptPath + " must define a wild_actions table");
    }

    const sol::table actions = actionRoot.as<sol::table>();
    if (!actions.get<sol::object>("wander").is<sol::protected_function>()) {
        throw std::runtime_error(scriptPath + " must define wild_actions.wander(context)");
    }
}

WildAi::~WildAi() = default;

WildIntent WildAi::decide(std::uint64_t entityId, std::uint16_t species, float x, float y,
                          float dt) {
    WildBrain& brain = brains_[entityId];
    if (!brain.hasHome) {
        brain.homeX = x;
        brain.homeY = y;
        brain.hasHome = true;
    }

    switch (brain.mode) {
        case WildMode::Wander:
            return tickWander(entityId, species, x, y, dt, brain);
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
    // math.randomseed 는 스크립트가 아니라 Lua 표준 라이브러리 것이다.
    // 여기서 부르지 않으면 --wild-seed 가 스폰 좌표만 고정하고 배회는 매번 다르다.
    (*lua_)["math"]["randomseed"](value);
}

WildIntent WildAi::tickWander(std::uint64_t entityId, std::uint16_t species, float x, float y,
                              float dt, WildBrain& brain) {
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

    const WanderActionResult action = callWanderAction(entityId, species, x, y, brain);
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

WildAi::WanderActionResult WildAi::callWanderAction(std::uint64_t entityId,
                                                    std::uint16_t species, float x, float y,
                                                    const WildBrain& brain) {
    sol::table actions = (*lua_)["wild_actions"];
    sol::protected_function wander = actions["wander"];

    sol::table context = lua_->create_table_with(
        "id", entityId,
        "species", species,
        "x", x,
        "y", y,
        "home_x", brain.homeX,
        "home_y", brain.homeY);

    const sol::protected_function_result result = wander(context);
    if (!result.valid()) {
        const sol::error err = result;
        spdlog::debug("wild wander action failed for {}: {}", entityId, err.what());
        return {};
    }

    WanderActionResult action;
    const sol::object first = result.get<sol::object>(0);
    if (first.is<sol::table>()) {
        const sol::table table = first.as<sol::table>();

        sol::optional<float> targetX = table["target_x"];
        if (!targetX) {
            targetX = table["targetX"];
        }
        if (!targetX) {
            targetX = table["x"];
        }

        sol::optional<float> targetY = table["target_y"];
        if (!targetY) {
            targetY = table["targetY"];
        }
        if (!targetY) {
            targetY = table["y"];
        }

        if (!targetX || !targetY) {
            return {};
        }

        action.targetX = *targetX;
        action.targetY = *targetY;

        if (const sol::optional<float> acceptance = table["acceptance_radius"]) {
            action.acceptanceRadius = *acceptance;
        } else if (const sol::optional<float> camelAcceptance = table["acceptanceRadius"]) {
            action.acceptanceRadius = *camelAcceptance;
        }

        if (const sol::optional<float> rest = table["rest_seconds"]) {
            action.restAfterArriveSeconds = *rest;
        } else if (const sol::optional<float> camelRest = table["restSeconds"]) {
            action.restAfterArriveSeconds = *camelRest;
        } else if (const sol::optional<float> shortRest = table["rest"]) {
            action.restAfterArriveSeconds = *shortRest;
        }
    } else {
        // 간단한 action 은 예전 wild_tick 처럼 (x, y) 만 돌려줘도 된다.
        const sol::optional<float> targetX = result.get<sol::optional<float>>(0);
        const sol::optional<float> targetY = result.get<sol::optional<float>>(1);
        if (!targetX || !targetY) {
            return {};
        }
        action.targetX = *targetX;
        action.targetY = *targetY;
    }

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

}  // namespace heaven::field
