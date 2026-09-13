#include "WildBt.h"

#include <spdlog/spdlog.h>
#include <sol/sol.hpp>
#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace heaven::instance {

namespace {

const char* stateName(WildPokemonAIState state) {
    return state == WildPokemonAIState::Wander ? "wander" : "battle";
}

float readFloat(const sol::table& table, const char* name, float fallback) {
    const sol::optional<float> value = table[name];
    return value && std::isfinite(*value) ? *value : fallback;
}

} // namespace

WildBt::WildBt(const std::string& scriptPath) : lua_(std::make_unique<sol::state>()) {
    lua_->open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::package);

    // 하위 행동 모듈은 작업 디렉터리가 아니라 진입 스크립트 옆에서 찾는다.
    const auto directory = std::filesystem::absolute(scriptPath).parent_path().generic_string();
    sol::table package = (*lua_)["package"];
    package["path"] = directory + "/?.lua;" + package.get<std::string>("path");

    const auto loaded = lua_->safe_script_file(scriptPath, sol::script_pass_on_error);
    if (!loaded.valid()) {
        const sol::error error = loaded;
        throw std::runtime_error("cannot load wild AI script " + scriptPath + ": " + error.what());
    }

    const sol::object rootObject = (*lua_)["wild_ai"];
    if (!rootObject.is<sol::table>()) {
        throw std::runtime_error(scriptPath + " must define a wild_ai table");
    }

    const auto root = rootObject.as<sol::table>();
    for (const char* name : {"wander", "battle"}) {
        if (root.get<sol::object>(name).get_type() != sol::type::function) {
            throw std::runtime_error(scriptPath + " must define wild_ai." + name + "(context)");
        }
    }
}

WildBt::~WildBt() = default;

WildDecision WildBt::decide(const WildBtContext& context) {
    try {
        const WildPokemonAIActionMemory emptyMemory;
        const auto& memory = context.memory ? *context.memory : emptyMemory;
        const auto& movement = memory.movement;
        auto playerList = lua_->create_table();
        int playerIndex = 1;

        if (context.players) {
            for (const auto& player : *context.players) {
                if (player.mapId != context.mapId || !std::isfinite(player.x) ||
                    !std::isfinite(player.y) || !std::isfinite(player.z)) {
                    continue;
                }

                playerList[playerIndex++] = lua_->create_table_with(
                    "id", player.entityId, "map_id", player.mapId,
                    "x", player.x, "y", player.y, "z", player.z);
            }
        }

        auto luaContext = lua_->create_table_with(
            "entity_id", context.entityId,
            "species", context.species,
            "map_id", context.mapId,
            "state", stateName(context.state),
            "x", context.position.x,
            "y", context.position.y,
            "z", context.position.z,
            "players", playerList,
            "target_id", memory.targetId,
            "moving", movement.moving,
            "rest_remaining", movement.restRemaining,
            "replan_remaining", movement.replanRemaining,
            "attack_cooldown", memory.attackCooldown,
            "path_target_x", movement.requestedGoal.x,
            "path_target_y", movement.requestedGoal.y);

        sol::table root = (*lua_)["wild_ai"];
        sol::protected_function decide = root[stateName(context.state)];
        const auto result = decide(luaContext);
        if (!result.valid()) {
            const sol::error error = result;
            spdlog::debug("wild AI {} failed for {}: {}", stateName(context.state),
                         context.entityId, error.what());
            return {};
        }

        const sol::object value = result.get<sol::object>();
        if (!value.is<sol::table>()) {
            return {};
        }

        const auto table = value.as<sol::table>();
        WildDecision decision;
        decision.nextState = context.state;
        const sol::optional<std::string> nextState = table["next_state"];
        if (nextState) {
            if (*nextState == "wander") {
                decision.nextState = WildPokemonAIState::Wander;
            } else if (*nextState == "battle") {
                decision.nextState = WildPokemonAIState::Battle;
            } else {
                return {};
            }
        }

        const sol::optional<std::string> action = table["action"];
        if (!action) {
            return {};
        }

        if (*action == "continue") {
            decision.action = WildDecision::Action::Continue;
        } else if (*action == "wait") {
            decision.action = WildDecision::Action::Wait;
        } else if (*action == "wander") {
            decision.action = WildDecision::Action::Wander;
        } else if (*action == "chase") {
            decision.action = WildDecision::Action::Chase;
        } else if (*action == "attack") {
            decision.action = WildDecision::Action::Attack;
        } else {
            return {};
        }

        decision.targetId = table.get_or<std::uint64_t>("target_id", 0);
        decision.moveSeconds = readFloat(table, "move_seconds", decision.moveSeconds);
        decision.attackRange = readFloat(table, "attack_range", decision.attackRange);
        decision.acceptanceRadius = readFloat(table, "acceptance_radius", decision.acceptanceRadius);
        decision.restSeconds = readFloat(table, "rest_seconds", decision.restSeconds);
        decision.reconsiderSeconds = readFloat(table, "reconsider_seconds", decision.reconsiderSeconds);
        decision.valid = true;
        return decision;
    } catch (const std::exception& error) {
        spdlog::debug("invalid wild AI result for {}: {}", context.entityId, error.what());
        return {};
    }
}

void WildBt::seed(unsigned value) {
    if (value != 0) {
        (*lua_)["math"]["randomseed"](value);
    }
}

} // namespace heaven::instance
