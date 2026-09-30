#include "../InstanceWeather.h"
#include "WeatherMath.h"
namespace heaven::instance {
void InstanceWeather::updateDesert(double dt) {
    const auto& p=profile_.environment;
    const double dryness=1-earth::clamp01((ground_.waterKgM2+ground_.soilKgM2)/p.soilCapacityKgM2);
    const double wind=earth::clamp01((windSpeedMps_-p.dustStartWindMps)/(p.dustFullWindMps-p.dustStartWindMps));
    // 젖은 모래와 눈/얼음으로 덮인 지면은 잘 날리지 않는다. 먼지가 생기고 가라앉는 데도 시간이 걸린다.
    const double cover=1-earth::clamp01(ground_.snowKgM2+ground_.iceKgM2);
    const double drySurface=1-earth::clamp01((ground_.waterKgM2+ground_.filmKgM2)/p.surfaceFilmCapacityKgM2);
    const double target=p.sandAvailability*dryness*dryness*drySurface*wind*cover;
    environment_.sandstormIntensity=earth::relax(environment_.sandstormIntensity,target,dt,p.dustResponseSeconds);
}
}
