#include "../InstanceWeather.h"
#include "WeatherMath.h"
namespace heaven::instance {
void InstanceWeather::updateDesert(double dt) {
    const auto& p=profile_.environment;
    const auto fuzzy = evaluateFuzzyWeather(p.fuzzyWeather, fuzzyWeatherInputs());
    // 젖은 모래와 눈/얼음으로 덮인 지면은 잘 날리지 않는다. 먼지가 생기고 가라앉는 데도 시간이 걸린다.
    const double cover=1-earth::clamp01(ground_.snowKgM2+ground_.iceKgM2);
    const double target=p.sandAvailability*fuzzy.sandstormStrength*cover;
    environment_.sandstormIntensity=earth::relax(environment_.sandstormIntensity,target,dt,p.dustResponseSeconds);
}
}
