#pragma once
#include "../InstanceWeather.h"
#include <algorithm>

namespace heaven::instance {
inline hhv::movement::Environment movementEnvironment(const EnvironmentProfile& p,const InstanceWeatherSnapshot& s) {
    hhv::movement::Environment value;
    value.speedMultiplier=static_cast<float>(s.environment.movementMultiplier);
    value.traction=static_cast<float>(1+(p.iceTractionMultiplier-1)*std::clamp(s.environment.iceMm,0.,1.));
    value.tideOffsetCm=static_cast<float>(s.environment.tideLevelM*100);
    value.swimSpeed=static_cast<float>(p.swimSpeedCmPerSecond);
    value.water=p.waterRegions;
    return value;
}
}
