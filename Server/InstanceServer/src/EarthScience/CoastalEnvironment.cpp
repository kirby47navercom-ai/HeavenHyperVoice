#include "../InstanceWeather.h"
#include "WeatherMath.h"
namespace heaven::instance {
void InstanceWeather::updateCoast(double dt) {
    using namespace earth;
    const auto& p=profile_.environment;
    const double springPhase=2*Pi*worldSimulationSeconds_/(p.springNeapPeriodDays*p.daySeconds)
        +p.springNeapPhaseDegrees*Pi/180;
    environment_.tideEnvelopeM=p.tideAmplitudeM*(p.neapTideFraction+
        (1-p.neapTideFraction)*(1+std::cos(springPhase))*.5);
    environment_.tideLevelM=environment_.tideEnvelopeM*std::sin(
        2*Pi*worldSimulationSeconds_/(p.tidePeriodHours*3600)+p.tidePhaseDegrees*Pi/180);
    environment_.shoreWaterDepthM=std::max(0.0,environment_.tideLevelM-p.shoreHeightM);
    environment_.waveHeightM=relax(environment_.waveHeightM,
        p.waveMaxHeightM*clamp01(windSpeedMps_/15),dt,p.waveResponseSeconds);
}
}
