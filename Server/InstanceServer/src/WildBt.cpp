#include "WildBt.h"

#include <spdlog/spdlog.h>
#include <sol/sol.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace heaven::instance {

namespace {

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::uint64_t readId(const sol::table& table, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        if (const sol::optional<std::uint64_t> id = table[name]) {
            return *id;
        }
    }
    return 0;
}

float readFloat(const sol::table& table, std::initializer_list<const char*> names,
                float fallback) {
    for (const char* name : names) {
        const sol::optional<float> value = table[name];
        if (value && std::isfinite(*value)) {
            return *value;
        }
    }
    return fallback;
}

sol::table makePlayerTable(sol::state& lua, const ObservedPlayer& player, float observerX,
                           float observerY) {
    const float dx = player.x - observerX;
    const float dy = player.y - observerY;
    return lua.create_table_with(
        "id", player.entityId,
        "map_id", player.mapId,
        "x", player.x,
        "y", player.y,
        "distance", std::sqrt(dx * dx + dy * dy));
}

}  // namespace

WildBt::WildBt(const std::string& scriptPath) : lua_(std::make_unique<sol::state>()) {
    lua_->open_libraries(sol::lib::base, sol::lib::math, sol::lib::table);

    const sol::protected_function_result loaded =
        lua_->safe_script_file(scriptPath, sol::script_pass_on_error);
    if (!loaded.valid()) {
        const sol::error err = loaded;
        throw std::runtime_error("cannot load wild AI script " + scriptPath + ": " +
                                 err.what());
    }

    const sol::object rootObject = lua_->get<sol::object>("wild_ai");
    if (!rootObject.is<sol::table>()) {
        throw std::runtime_error(scriptPath + " must define a wild_ai table");
    }

    const sol::table root = rootObject.as<sol::table>();
    if (root.get<sol::object>("decide").get_type() != sol::type::function) {
        throw std::runtime_error(scriptPath + " must define wild_ai.decide(context)");
    }
}

WildBt::~WildBt() = default;

WildDecision WildBt::decide(const WildBtContext& context) {
    sol::table root = (*lua_)["wild_ai"];
    sol::protected_function decide = root["decide"];

    sol::table luaContext = lua_->create_table_with(
        "entity_id", context.entityId,
        "species", context.species,
        "map_id", context.mapId,
        "x", context.x,
        "y", context.y,
        "aggro_radius", context.aggroRadius,
        "lose_target_radius", context.loseTargetRadius);

    if (context.currentTarget != nullptr) {
        luaContext["current_target"] =
            makePlayerTable(*lua_, *context.currentTarget, context.x, context.y);
    }
    if (context.nearestPlayer != nullptr) {
        luaContext["nearest_player"] =
            makePlayerTable(*lua_, *context.nearestPlayer, context.x, context.y);
    }

    const sol::protected_function_result result = decide(luaContext);
    if (!result.valid()) {
        const sol::error err = result;
        spdlog::debug("wild AI decision failed for {}: {}", context.entityId, err.what());
        return {};
    }

    const sol::object first = result.get<sol::object>(0);
    if (!first.is<sol::table>()) {
        return {};
    }

    const sol::table table = first.as<sol::table>();
    const sol::optional<std::string> actionValue = table["action"];
    if (!actionValue) {
        return {};
    }

    WildDecision decision;
    const std::string action = lowerAscii(*actionValue);
    if (action == "wander") {
        decision.action = WildDecision::Action::Wander;
        decision.valid = true;
    } else if (action == "chase") {
        decision.action = WildDecision::Action::Chase;
        decision.targetId = readId(table, {"target_id", "targetId"});
        decision.valid = decision.targetId != 0;
    } else {
        spdlog::debug("wild AI returned unknown action: {}", action);
        return {};
    }

    decision.wanderRadius = readFloat(table, {"wander_radius", "wanderRadius", "radius"},
                                      decision.wanderRadius);
    decision.acceptanceRadius = readFloat(table, {"acceptance_radius", "acceptanceRadius"},
                                          decision.acceptanceRadius);
    decision.restSeconds = readFloat(table, {"rest_seconds", "restSeconds"},
                                     decision.restSeconds);
    decision.reconsiderSeconds = readFloat(table, {"reconsider_seconds", "reconsiderSeconds"},
                                           decision.reconsiderSeconds);
    return decision;
}

void WildBt::seed(unsigned value) {
    if (value == 0) {
        return;
    }
    (*lua_)["math"]["randomseed"](value);
}

}  // namespace heaven::instance
