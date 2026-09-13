#include "WildPokemonAIFSM.h"

namespace heaven::instance {

WildPokemonAIState WildPokemonAIFSM::state() const {
    return state_;
}

bool WildPokemonAIFSM::transitionTo(WildPokemonAIState nextState) {
    if (state_ == nextState) {
        return false;
    }

    state_ = nextState;
    return true;
}

} // namespace heaven::instance
