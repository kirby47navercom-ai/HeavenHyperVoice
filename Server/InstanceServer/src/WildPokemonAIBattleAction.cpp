#include "WildPokemonAIBattleAction.h"

namespace heaven::instance {

WildIntent WildPokemonAIBattleAction::execute(const WildDecision& decision,
                                              const WildPokemonAIActionContext& context,
                                              WildPokemonAIActionMemory& memory) const {
    auto& movement = memory.movement;
    if (decision.action == WildDecision::Action::Continue) {
        return movement.follow(context.map, context.position);
    }

    if (decision.action == WildDecision::Action::Wait) {
        movement.stop();
        return {};
    }

    // 대상 선택과 거리 조건은 Lua가 판단한다. C++는 현재 방의 유효한 대상인지 확인한다.
    const auto* target = findActionTarget(context, decision.targetId);
    if (!target) {
        movement.stop();
        return {};
    }

    memory.targetId = target->entityId;
    if (decision.action == WildDecision::Action::Attack) {
        movement.stop();
        if (memory.attackCooldown > 0.f) {
            return {};
        }

        WildIntent intent;
        intent.attacking = true;
        intent.attackTargetId = target->entityId;
        intent.attackRange = boundedAIValue(decision.attackRange, 180.f, 30.f, 600.f);
        memory.attackCooldown = boundedAIValue(decision.reconsiderSeconds, 1.2f, 0.1f, 2.f);
        return intent;
    }

    if (decision.action == WildDecision::Action::Chase) {
        const float acceptance = boundedAIValue(decision.acceptanceRadius, 140.f, 30.f, 600.f);
        const float reconsider = boundedAIValue(decision.reconsiderSeconds, 0.5f, 0.1f, 2.f);
        if (movement.begin(context.map, context.position, {target->x, target->y, target->z},
                           acceptance, reconsider)) {
            movement.replanRemaining = reconsider;
            return movement.follow(context.map, context.position);
        }

        movement.restRemaining = 0.5f;
    }

    return {};
}

} // namespace heaven::instance
