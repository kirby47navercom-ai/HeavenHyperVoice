#pragma once

#include "WildPokemonAIAction.h"
#include "WildPokemonAIFSM.h"
#include <memory>
#include <random>
#include <unordered_map>

namespace heaven::instance {

class WildBt;

// 방의 AI 연결부. 현재 상태의 Lua BT를 평가하고 결과를 FSM/행동 실행기에 전달한다.
// 상태별 전환 조건은 포함하지 않는다. 한 방의 틱 스레드에서만 호출한다.
class WildAi {
public:
    explicit WildAi(std::unique_ptr<WildBt> behavior);
    ~WildAi();

    WildAi(const WildAi&) = delete;
    WildAi& operator=(const WildAi&) = delete;

    WildIntent decide(std::uint64_t entityId, std::uint16_t species, std::uint32_t mapId,
                      float x, float y, float z, float dt,
                      const std::vector<ObservedPlayer>& players);
    void setArea(const WildArea& area);
    void setMap(const Map* map);
    void notifyMoveBlocked(std::uint64_t entityId);
    void seed(unsigned value);
    WildPokemonAIState stateOf(std::uint64_t entityId) const;

private:
    struct WildBrain {
        WildPokemonAIFSM fsm;
        WildPokemonAIActionMemory action;
    };

    std::unique_ptr<WildBt> behavior_;
    std::mt19937 random_;
    WildArea area_;
    const Map* map_ = nullptr;
    std::unordered_map<std::uint64_t, WildBrain> brains_;
};

} // namespace heaven::instance
