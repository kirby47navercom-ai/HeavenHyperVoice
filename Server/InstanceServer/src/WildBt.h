#pragma once

#include "WildPokemonAIAction.h"
#include <memory>
#include <string>

namespace sol { class state; }

namespace heaven::instance {

struct WildBtContext {
    std::uint64_t entityId = 0;
    std::uint16_t species = 0;
    std::uint32_t mapId = 0;
    nav::Vec3 position;
    WildPokemonAIState state = WildPokemonAIState::Wander;
    const WildPokemonAIActionMemory* memory = nullptr;
    const std::vector<ObservedPlayer>* players = nullptr;
};

class WildBt {
public:
    explicit WildBt(const std::string& scriptPath);
    ~WildBt();

    WildDecision decide(const WildBtContext& context);
    void seed(unsigned value);

private:
    std::unique_ptr<sol::state> lua_;
};

} // namespace heaven::instance
