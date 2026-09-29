#include "../InstanceWeather.h"
#include "WeatherMath.h"
namespace heaven::instance {
void InstanceWeather::updateCoast(double dt) {
    const auto& p=profile_.environment;
    // 외해는 열린 경계다. 조석은 육상 물 저장량에서 물을 빼지 않는 외부 수위 조건이다.
    const double angle=2*earth::Pi*simulationTimeSeconds_/(p.tidePeriodHours*3600)+p.tidePhaseDegrees*earth::Pi/180;
    environment_.tideLevelM=p.tideAmplitudeM*std::sin(angle);
    const double target=p.tideAmplitudeM>0 ? p.waveMaxHeightM*earth::clamp01(windSpeedMps_/15) : 0;
    // 바람이 멎어도 파도 에너지는 바로 사라지지 않는다.
    environment_.waveHeightM=earth::relax(environment_.waveHeightM,target,dt,p.waveResponseSeconds);
}
}
