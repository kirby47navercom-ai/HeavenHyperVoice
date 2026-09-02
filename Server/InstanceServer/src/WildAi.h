#pragma once

// 야생 포켓몬 AI 실행기.
//
// 행동 선택은 하나의 Lua BT(wild_ai.lua)가 맡고, C++ 은 현재 action 실행 상태와
// 서버 권위 검증(타깃/거리/navmesh/충돌/속도 적용)을 맡는다.
// Lua 는 처음, action 완료, 차단, 타깃 변화, 플레이어 접근 같은 재판단 지점에서만
// 호출하고 이동 중에는 기존 action 을 계속 수행한다.
//
// 스레드 안전하지 않다. 한 스레드(방 틱)에서만 부를 것.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

#include "AiTypes.h"
#include "Map.h"
#include "Path.h"

namespace heaven::instance {

class WildBt;

// 야생이 돌아다니는 구역. 방마다 다르다 — 맵 경계에서 뽑아 넣는다.
// 인스턴스는 필드와 월드 크기도 스폰 지점도 달라서 상수로 둘 수 없다.
struct WildArea {
    float centerX = 0.f;
    float centerY = 0.f;
    float halfExtent = 4000.f;
};

// 이번 틱에 이 포켓몬이 향할 목표점. moving 이 false 면 제자리다.
struct WildIntent {
    float targetX = 0.f;
    float targetY = 0.f;
    float acceptanceRadius = 80.f;
    bool moving = false;
};

class WildAi {
public:
    explicit WildAi(std::unique_ptr<WildBt> behavior);
    ~WildAi();

    WildAi(const WildAi&) = delete;
    WildAi& operator=(const WildAi&) = delete;

    // 포켓몬 하나의 다음 목표를 정한다. 이동 중이거나 휴식 중이면 실행 상태만
    // 전진하고, 새 행동 선택이 필요할 때만 Lua BT 를 호출한다.
    WildIntent decide(std::uint64_t entityId, std::uint16_t species, std::uint32_t mapId,
                      float x, float y, float dt, const std::vector<ObservedPlayer>& players);

    // 돌아다닐 구역. 방을 만들 때 한 번만 부를 것.
    void setArea(const WildArea& area) { area_ = area; }

    // 서버 맵. nullptr 이면 목표점으로 직선 이동한다.
    void setMap(const Map* map) {
        map_ = map;
        if (map_ != nullptr && map_->loaded()) {
            agent_ = map_->agent();
        }
    }

    // 월드가 목표 방향 이동을 navmesh 위에서 적용하지 못했을 때 부른다.
    void notifyMoveBlocked(std::uint64_t entityId);

    // 배회 경로를 재현 가능하게 만든다. 0 이면 생성자에서 만든 비결정 난수를
    // 그대로 쓴다. 기동 시 한 번만 부를 것.
    void seed(unsigned value);

private:
    enum class RunningAction : std::uint8_t {
        None,
        Wander,
        Chase
    };

    enum class WildPhase : std::uint8_t {
        NeedDecision,
        Moving,
        Resting
    };

    struct WildBrain {
        RunningAction action = RunningAction::None;
        WildPhase phase = WildPhase::NeedDecision;
        float targetX = 0.f;
        float targetY = 0.f;
        float acceptanceRadius = 80.f;
        float restAfterArriveSeconds = 1.f;
        float restRemainingSeconds = 0.f;
        std::vector<nav::Vec3> path;
        std::size_t pathIndex = 0;
        std::uint64_t targetId = 0;
        float replanRemainingSeconds = 0.f;
        float lastTargetX = 0.f;
        float lastTargetY = 0.f;
    };

    struct MoveAction {
        float targetX = 0.f;
        float targetY = 0.f;
        float acceptanceRadius = 80.f;
        float restAfterArriveSeconds = 1.f;
        bool valid = false;
    };

    WildIntent chooseNextAction(std::uint64_t entityId, std::uint16_t species,
                                std::uint32_t mapId, float x, float y, WildBrain& brain,
                                const std::vector<ObservedPlayer>& players);
    bool beginMove(float x, float y, const MoveAction& action, WildBrain& brain);
    WildIntent followPath(float x, float y, WildBrain& brain);
    MoveAction makeWanderAction(float x, float y, const WildDecision& decision);
    MoveAction makeChaseAction(const WildDecision& decision, const ObservedPlayer& target);
    void requestDecision(WildBrain& brain);
    void beginRest(WildBrain& brain, float seconds);
    static float distanceSquared(float ax, float ay, float bx, float by);

    std::unique_ptr<WildBt> behavior_;
    std::mt19937 rng_;
    WildArea area_;
    const Map* map_ = nullptr;
    Pathfinder pathfinder_;
    nav::Agent agent_;
    std::unordered_map<std::uint64_t, WildBrain> brains_;
};

}  // namespace heaven::instance
