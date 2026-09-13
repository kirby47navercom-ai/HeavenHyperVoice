#include "WildAi.h"

#include "WildBt.h"
#include "WildPokemonAIBattleAction.h"
#include "WildPokemonAIWanderAction.h"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace heaven::instance {

WildAi::WildAi(std::unique_ptr<WildBt> behavior)
    : behavior_(std::move(behavior)), random_(std::random_device{}()) {
    if (!behavior_) {
        throw std::invalid_argument("wild behavior tree is required");
    }
}

WildAi::~WildAi() = default;

void WildAi::setArea(const WildArea& area) {
    area_ = area;
}

void WildAi::setMap(const Map* map) {
    map_ = map;
}

void WildAi::seed(unsigned value) {
    if (value != 0) {
        random_.seed(value);
        behavior_->seed(value);
    }
}

WildPokemonAIState WildAi::stateOf(std::uint64_t entityId) const {
    const auto found = brains_.find(entityId);
    return found == brains_.end() ? WildPokemonAIState::Wander : found->second.fsm.state();
}

void WildAi::notifyMoveBlocked(std::uint64_t entityId) {
    const auto found = brains_.find(entityId);
    if (found != brains_.end()) {
        if (found->second.fsm.state() == WildPokemonAIState::Wander) {
            found->second.action.finishWander();
        } else {
            found->second.action.movement.blocked();
        }
    }
}

WildIntent WildAi::decide(std::uint64_t entityId, std::uint16_t species, std::uint32_t mapId,
                          float x, float y, float z, float dt,
                          const std::vector<ObservedPlayer>& players) {
    auto& brain = brains_[entityId];
    const float elapsed = std::isfinite(dt) ? std::max(dt, 0.f) : 0.f;
    brain.action.advanceTime(elapsed);

    const WildPokemonAIActionContext actionContext{{x, y, z}, map_, area_, players, mapId};
    WildBtContext context;
    context.entityId = entityId;
    context.species = species;
    context.mapId = mapId;
    context.position = actionContext.position;
    context.state = brain.fsm.state();
    context.memory = &brain.action;
    context.players = &players;

    const WildDecision decision = behavior_->decide(context);
    if (!decision.valid) {
        brain.action.movement.stop();
        return {};
    }

    if (decision.nextState != brain.fsm.state()) {
        if (decision.targetId != 0 && !findActionTarget(actionContext, decision.targetId)) {
            return {};
        }

        brain.fsm.transitionTo(decision.nextState);
        // 전환 전 경로/대기 시간을 새 행동에 넘기지 않는다.
        brain.action = {};
        brain.action.targetId = decision.targetId;
        // 새 상태의 BT는 다음 틱에 실행한다. 틱 안의 연쇄 전환을 방지한다.
        return {};
    }

    switch (brain.fsm.state()) {
    case WildPokemonAIState::Wander:
        return WildPokemonAIWanderAction{}.execute(decision, actionContext, brain.action, random_);
    case WildPokemonAIState::Battle:
        return WildPokemonAIBattleAction{}.execute(decision, actionContext, brain.action);
    }

    return {};
}

} // namespace heaven::instance
