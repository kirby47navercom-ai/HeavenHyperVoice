#pragma once

#include "WildPokemonAIAction.h"

namespace heaven::instance {

class WildPokemonAIBattleAction {
public:
    WildIntent execute(const WildDecision& decision, const WildPokemonAIActionContext& context,
                       WildPokemonAIActionMemory& memory) const;
};

} // namespace heaven::instance
