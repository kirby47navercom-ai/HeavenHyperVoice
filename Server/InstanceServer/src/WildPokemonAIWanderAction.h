#pragma once

#include "WildPokemonAIAction.h"

namespace heaven::instance {

class WildPokemonAIWanderAction {
public:
    WildIntent execute(const WildDecision& decision, const WildPokemonAIActionContext& context,
                       WildPokemonAIActionMemory& memory, std::mt19937& random) const;
};

} // namespace heaven::instance
