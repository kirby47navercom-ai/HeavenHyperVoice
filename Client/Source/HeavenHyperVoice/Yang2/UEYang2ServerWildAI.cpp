// YANG2_CLIENT_AUTHORITY_ONLY: 서버의 FSM/행동 실행기와 같은 구현이에요.
#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "Offline wild AI must not compile on main."
#endif
#include "../../../../Server/InstanceServer/src/WildAi.cpp"
#include "../../../../Server/InstanceServer/src/WildPokemonAIFSM.cpp"
#include "../../../../Server/InstanceServer/src/WildPokemonAIAction.cpp"
#include "../../../../Server/InstanceServer/src/WildPokemonAIWanderAction.cpp"
#include "../../../../Server/InstanceServer/src/WildPokemonAIBattleAction.cpp"
#include "../../../../Server/FieldShared/src/PokemonMovement.cpp"
