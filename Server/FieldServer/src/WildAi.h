#pragma once

// 야생 포켓몬의 행동 FSM.
//
// C++ 이 개체별 상태(이동 중/휴식 중/다음 행동 필요)를 들고, Lua 는 행동이
// 새로 필요할 때만 호출된다. 현재는 wander action 만 있다. Lua 는 "어디로
// 가고 싶은가" 같은 행동 결정을 돌려주고, C++ FSM 이 그 결과를 목표 달성이나
// 충돌 이벤트까지 유지한다.
//
// 스레드 안전하지 않다. Lua VM 하나를 그대로 노출하므로 한 스레드(필드 틱)에서만
// 부를 것. 기동 시 스크립트를 못 읽으면 예외를 던지지만, 런타임 action 오류는
// "제자리에 서 있어라" 로 처리한다 — 스크립트 하나가 서버를 멈추면 안 된다.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Map.h"
#include "Path.h"

namespace sol { class state; }

namespace heaven::field {

// 이번 틱에 이 포켓몬이 향할 목표점. moving 이 false 면 제자리다.
struct WildIntent {
    float targetX = 0.f;
    float targetY = 0.f;
    float acceptanceRadius = 80.f;
    bool moving = false;
};

class WildAi {
public:
    // 스크립트를 읽고 실행한다. 실패하면 예외를 던진다 (기동 시점에만).
    explicit WildAi(const std::string& scriptPath);
    ~WildAi();

    WildAi(const WildAi&) = delete;
    WildAi& operator=(const WildAi&) = delete;

    // 포켓몬 하나의 다음 목표를 정한다. 이동 중이거나 휴식 중이면 Lua 를 부르지
    // 않고 FSM 상태만 전진한다. 새 wander action 이 필요할 때만 Lua 를 부른다.
    WildIntent decide(std::uint64_t entityId, std::uint16_t species, float x, float y,
                      float dt);

    // 서버 맵. nullptr 이면 예전처럼 Lua 목표점으로 직선 이동한다.
    void setMap(const Map* map) {
        map_ = map;
        if (map_ != nullptr && map_->loaded()) {
            agent_ = map_->agent();
        }
    }

    // 월드가 목표 방향 이동을 navmesh 위에서 적용하지 못했을 때 부른다.
    void notifyMoveBlocked(std::uint64_t entityId);

    // 배회 경로를 재현 가능하게 만든다. 0 이면 아무것도 하지 않는다 (Lua 기본
    // 시딩을 그대로 둔다). 기동 시 한 번만 부를 것.
    void seed(unsigned value);

private:
    enum class WildMode : std::uint8_t {
        Wander,
        Combat,
        Downed
    };

    enum class WildPhase : std::uint8_t {
        NeedAction,
        Moving,
        Resting
    };

    struct WildBrain {
        WildMode mode = WildMode::Wander;
        WildPhase phase = WildPhase::NeedAction;
        float homeX = 0.f;
        float homeY = 0.f;
        float targetX = 0.f;
        float targetY = 0.f;
        float acceptanceRadius = 80.f;
        float restAfterArriveSeconds = 1.f;
        float restRemainingSeconds = 0.f;
        std::vector<nav::Vec3> path;
        std::size_t pathIndex = 0;
        bool hasHome = false;
    };

    struct WanderActionResult {
        float targetX = 0.f;
        float targetY = 0.f;
        float acceptanceRadius = 80.f;
        float restAfterArriveSeconds = 1.f;
        bool valid = false;
    };

    WildIntent tickWander(std::uint64_t entityId, std::uint16_t species, float x, float y,
                          float dt, WildBrain& brain);
    bool beginWanderMove(float x, float y, const WanderActionResult& action, WildBrain& brain);
    WildIntent followWanderPath(float x, float y, WildBrain& brain);
    WanderActionResult callWanderAction(std::uint64_t entityId, std::uint16_t species,
                                        float x, float y, const WildBrain& brain);
    void beginRest(WildBrain& brain, float seconds);
    static float distanceSquared(float ax, float ay, float bx, float by);

    std::unique_ptr<sol::state> lua_;
    const Map* map_ = nullptr;
    Pathfinder pathfinder_;
    nav::Agent agent_;
    std::unordered_map<std::uint64_t, WildBrain> brains_;
};

}  // namespace heaven::field
