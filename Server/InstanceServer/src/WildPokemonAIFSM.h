#pragma once

#include <cstdint>

namespace heaven::instance {

enum class WildPokemonAIState : std::uint8_t {
    Wander,
    Battle
};

// 상태를 보관하고 요청된 전환만 적용한다. 전환 조건은 Lua BT가 판단한다.
class WildPokemonAIFSM {
public:
    WildPokemonAIState state() const;
    bool transitionTo(WildPokemonAIState nextState);

private:
    WildPokemonAIState state_ = WildPokemonAIState::Wander;
};

} // namespace heaven::instance
