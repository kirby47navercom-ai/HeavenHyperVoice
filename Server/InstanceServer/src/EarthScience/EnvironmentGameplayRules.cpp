#include "EnvironmentGameplayRules.h"
#include "../InstanceWeather.h"
#include "WeatherMath.h"
namespace heaven::instance {
double environmentSpawnWeight(const EnvironmentSpawnRule& r,const InstanceWeatherSnapshot& w) {
    using namespace earth;
    const double precipitation=clamp01(w.precipitationMmPerHour/10);
    const double snow=clamp01((1-w.temperatureC)/2);
    const double night=clamp01(-w.environment.sunElevationDegrees/12);
    // 경계에서 종족이 갑자기 갈리는 대신 비/눈/밤의 정도에 따라 가중치를 보간해요.
    return std::max(0.0,r.baseWeight)*(1+(r.rainMultiplier-1)*precipitation*(1-snow))*
        (1+(r.snowMultiplier-1)*precipitation*snow)*(1+(r.nightMultiplier-1)*night);
}
void applyEnvironmentGameplay(const EnvironmentProfile& p,InstanceWeatherSnapshot& w) {
    const double ice=earth::clamp01(w.environment.iceMm);
    w.environment.movementMultiplier=(1+(p.wetMovementMultiplier-1)*w.groundWetness)*
        (1+(p.iceMovementMultiplier-1)*ice);
    w.environment.visibilityMultiplier=1+(p.sandVisibilityMultiplier-1)*w.environment.sandstormIntensity;
}
}
