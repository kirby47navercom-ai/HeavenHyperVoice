#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "AiTypes.h"

namespace sol { class state; }

namespace heaven::instance {

struct WildBtContext {
    std::uint64_t entityId = 0;
    std::uint16_t species = 0;
    std::uint32_t mapId = 0;
    float x = 0.f;
    float y = 0.f;
    const ObservedPlayer* currentTarget = nullptr;
    const ObservedPlayer* nearestPlayer = nullptr;
    float aggroRadius = 900.f;
    float loseTargetRadius = 1800.f;
};

class WildBt {
public:
    explicit WildBt(const std::string& scriptPath);
    ~WildBt();

    WildBt(const WildBt&) = delete;
    WildBt& operator=(const WildBt&) = delete;

    WildDecision decide(const WildBtContext& context);
    void seed(unsigned value);

private:
    std::unique_ptr<sol::state> lua_;
};

}  // namespace heaven::instance
