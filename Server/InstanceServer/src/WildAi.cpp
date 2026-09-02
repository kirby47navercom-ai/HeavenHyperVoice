#include "WildAi.h"

#include <spdlog/spdlog.h>
#include <sol/sol.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "InstanceGeometry.h"

namespace heaven::instance {

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

    return tickWander(entityId, species, x, y, dt, brain);
}

void WildAi::notifyMoveBlocked(std::uint64_t entityId) {
    const auto it = brains_.find(entityId);
    if (it == brains_.end()) {
        return;
    }

    // 막힌 목표를 계속 붙잡으면 벽 앞에서 매 틱 같은 이동을 시도한다.
    // 짧게 쉰 뒤 새 wander action 을 뽑는다.
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
        if (distanceSquared(x, y, brain.targetX, brain.targetY) <=
            brain.acceptanceRadius * brain.acceptanceRadius) {
            beginRest(brain, brain.restAfterArriveSeconds);
            return {};
        }

        return WildIntent{brain.targetX, brain.targetY, true};
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

    brain.targetX = action.targetX;
    brain.targetY = action.targetY;
    brain.acceptanceRadius = std::max(action.acceptanceRadius, 1.f);
    brain.restAfterArriveSeconds = std::max(action.restAfterArriveSeconds, 0.f);
    brain.phase = WildPhase::Moving;

    if (distanceSquared(x, y, brain.targetX, brain.targetY) <=
        brain.acceptanceRadius * brain.acceptanceRadius) {
        beginRest(brain, brain.restAfterArriveSeconds);
        return {};
    }

    return WildIntent{brain.targetX, brain.targetY, true};
}

WildAi::WanderActionResult WildAi::callWanderAction(std::uint64_t entityId,
                                                    std::uint16_t species, float x, float y,
                                                    const WildBrain& brain) {
    sol::table actions = (*lua_)["wild_actions"];
    sol::protected_function wander = actions["wander"];

    // 구역은 C++ 이 정한다 (맵의 경계 구에서 나온다). Lua 에 상수로 두면
    // 맵이 바뀔 때마다 스크립트를 같이 고쳐야 하고, 빠뜨리면 야생이 맵 밖
    // 경계에 몰려 선다.
    sol::table context = lua_->create_table_with(
        "id", entityId,
        "species", species,
        "x", x,
        "y", y,
        "home_x", brain.homeX,
        "home_y", brain.homeY,
        "area_center_x", area_.centerX,
        "area_center_y", area_.centerY,
        "area_half_extent", area_.halfExtent);

    const sol::protected_function_result result = wander(context);
    if (!result.valid()) {
        const sol::error err = result;
        spdlog::debug("wild wander action failed for {}: {}", entityId, err.what());
        return {};
    }

    const sol::object first = result.get<sol::object>(0);
    if (!first.is<sol::table>()) {
        return {};
    }
    const sol::table table = first.as<sol::table>();

    const sol::optional<float> targetX = table["target_x"];
    const sol::optional<float> targetY = table["target_y"];
    if (!targetX || !targetY) {
        return {};
    }

    WanderActionResult action;
    action.targetX = *targetX;
    action.targetY = *targetY;
    if (const sol::optional<float> acceptance = table["acceptance_radius"]) {
        action.acceptanceRadius = *acceptance;
    }
    if (const sol::optional<float> rest = table["rest_seconds"]) {
        action.restAfterArriveSeconds = *rest;
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

}  // namespace heaven::instance
