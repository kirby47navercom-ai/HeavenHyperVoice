#pragma once

// 야생 포켓몬의 행동 결정을 Lua 스크립트에 맡긴다.
//
// C++ 은 "어떤 포켓몬이 지금 어디에 있다" 만 넘기고, Lua 가 "어디로 가고 싶은가"
// 를 돌려준다. 행동 트리(배회 -> 대기 -> 다음 목표)는 전적으로 Lua 안에 있고,
// 규칙을 바꿀 때 C++ 을 다시 빌드할 필요가 없다.
//
// 스레드 안전하지 않다. Lua VM 하나를 그대로 노출하므로 한 스레드(필드 틱)에서만
// 부를 것. 스크립트 오류는 던지지 않고 "제자리에 서 있어라" 로 처리한다 —
// 스크립트 하나가 서버를 멈추면 안 된다.

#include <cstdint>
#include <memory>
#include <string>

namespace sol { class state; }

namespace heaven::field {

// 이번 틱에 이 포켓몬이 향할 목표점. moving 이 false 면 제자리다.
struct WildIntent {
    float targetX = 0.f;
    float targetY = 0.f;
    bool moving = false;
};

class WildAi {
public:
    // 스크립트를 읽고 실행한다. 실패하면 예외를 던진다 (기동 시점에만).
    explicit WildAi(const std::string& scriptPath);
    ~WildAi();

    WildAi(const WildAi&) = delete;
    WildAi& operator=(const WildAi&) = delete;

    // 포켓몬 하나의 다음 목표를 정한다. 스크립트의 wild_tick(id, species, x, y, dt)
    // 를 부른다. 오류나 잘못된 반환이면 moving=false.
    WildIntent decide(std::uint64_t entityId, std::uint16_t species, float x, float y,
                      float dt);

    // 배회 경로를 재현 가능하게 만든다. 0 이면 아무것도 하지 않는다 (Lua 기본
    // 시딩을 그대로 둔다). 기동 시 한 번만 부를 것.
    void seed(unsigned value);

private:
    std::unique_ptr<sol::state> lua_;
};

}  // namespace heaven::field
