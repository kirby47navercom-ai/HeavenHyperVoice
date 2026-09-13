#include "WildPokemonAIWanderAction.h"
#include <algorithm>
#include <cmath>

namespace heaven::instance {

WildIntent WildPokemonAIWanderAction::execute(const WildDecision& decision,
                                              const WildPokemonAIActionContext& context,
                                              WildPokemonAIActionMemory& memory,
                                              std::mt19937& random) const {
    auto& movement = memory.movement;
    if (decision.action == WildDecision::Action::Wander) {
        movement.stop();
        std::uniform_real_distribution<float> angleDistribution(0.f, 6.2831853f);
        const float angle = angleDistribution(random);

        memory.wanderDirection = {std::cos(angle), std::sin(angle), 0.f};
        memory.wanderMoveRemaining = boundedAIValue(decision.moveSeconds, 2.f, 0.1f, 10.f);
        memory.wanderRestSeconds = boundedAIValue(decision.restSeconds, 2.f, 0.1f, 8.f);
        movement.restRemaining = 0.f;
        movement.moving = true;
    } else if (decision.action != WildDecision::Action::Continue) {
        movement.stop();
        return {};
    }

    if (!movement.moving) {
        return {};
    }

    if (memory.wanderMoveRemaining <= 0.f) {
        memory.finishWander();
        return {};
    }

    // 목표점은 방향 입력을 만들기 위한 점이다. 지형 검사나 A* 탐색은 하지 않는다.
    // 같은 방향을 유지하면서 활동 영역의 경계까지만 목표를 잡는다.
    const auto direction = memory.wanderDirection;
    const auto position = context.position;
    const auto& area = context.area;
    float distance = 100.f;

    const auto limitToBoundary = [&distance](float coordinate, float component, float center, float extent) {
        if (std::abs(component) > 0.0001f) {
            const float boundary = center + (component > 0.f ? extent : -extent);
            distance = std::min(distance, (boundary - coordinate) / component);
        }
    };
    limitToBoundary(position.x, direction.x, area.centerX, area.halfExtent);
    limitToBoundary(position.y, direction.y, area.centerY, area.halfExtent);

    if (distance <= 1.f) {
        memory.finishWander();
        return {};
    }

    WildIntent intent;
    intent.targetX = position.x + direction.x * distance;
    intent.targetY = position.y + direction.y * distance;
    intent.moving = true;
    return intent;
}

} // namespace heaven::instance
