#include "WildAi.h"

#include <spdlog/spdlog.h>
#include <sol/sol.hpp>

#include <stdexcept>

namespace heaven::field {

WildAi::WildAi(const std::string& scriptPath) : lua_(std::make_unique<sol::state>()) {
    lua_->open_libraries(sol::lib::base, sol::lib::math, sol::lib::table);

    const sol::protected_function_result loaded = lua_->safe_script_file(
        scriptPath, sol::script_pass_on_error);
    if (!loaded.valid()) {
        const sol::error err = loaded;
        throw std::runtime_error("cannot load wild AI script " + scriptPath + ": " + err.what());
    }

    if (!lua_->get<sol::object>("wild_tick").is<sol::protected_function>()) {
        throw std::runtime_error(scriptPath + " must define a function wild_tick(id, species, x, y, dt)");
    }
}

WildAi::~WildAi() = default;

WildIntent WildAi::decide(std::uint64_t entityId, std::uint16_t species, float x, float y,
                          float dt) {
    sol::protected_function tick = (*lua_)["wild_tick"];
    const sol::protected_function_result result = tick(entityId, species, x, y, dt);
    if (!result.valid()) {
        const sol::error err = result;
        // 초당 20회라 로그가 잠기지 않게 debug 로 둔다. 스크립트가 고장나면
        // 모든 야생이 제자리에 서므로 눈으로도 드러난다.
        spdlog::debug("wild_tick failed for {}: {}", entityId, err.what());
        return {};
    }

    // 스크립트는 목표를 원할 때만 (x, y) 를 돌려준다. 그 외에는 제자리.
    const sol::optional<float> tx = result.get<sol::optional<float>>(0);
    const sol::optional<float> ty = result.get<sol::optional<float>>(1);
    if (!tx || !ty) {
        return {};
    }
    return WildIntent{*tx, *ty, true};
}

void WildAi::seed(unsigned value) {
    if (value == 0) {
        return;
    }
    // math.randomseed 는 스크립트가 아니라 Lua 표준 라이브러리 것이다.
    // 여기서 부르지 않으면 --wild-seed 가 스폰 좌표만 고정하고 배회는 매번 다르다.
    (*lua_)["math"]["randomseed"](value);
}

}  // namespace heaven::field
