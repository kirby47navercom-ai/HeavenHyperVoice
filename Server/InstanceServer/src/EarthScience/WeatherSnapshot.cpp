#include "../InstanceWeather.h"
#include "WeatherMath.h"
namespace heaven::instance {
using namespace earth;
// 모든 계산이 끝난 방 상태를 네트워크용 값으로 변환한다.

double InstanceWeather::totalWaterKgM2() const {
    return nearAir_.vaporKgM2 + nearAir_.liquidKgM2 + upperAir_.vaporKgM2 +
           upperAir_.liquidKgM2 + ground_.waterKgM2 + ground_.soilKgM2 +
           ground_.snowKgM2 + ground_.iceKgM2 + ground_.filmKgM2;
}

InstanceWeatherSnapshot InstanceWeather::snapshot() const {
    InstanceWeatherSnapshot result;
    result.environment=environment_;
    result.environment.importedWaterKgM2=importedWaterKgM2_;
    result.environment.exportedWaterKgM2=exportedWaterKgM2_;
    result.environment.groundTemperatureC=ground_.temperatureC;
    result.environment.surfaceWaterMm=ground_.waterKgM2;
    result.environment.iceMm=ground_.iceKgM2;
    result.environment.soilMoisture=clamp01(ground_.soilKgM2/profile_.environment.soilCapacityKgM2);
    result.roomId = roomId_;
    result.revision = revision_;
    result.simulationTimeSeconds = simulationTimeSeconds_;
    result.temperatureC = static_cast<float>(nearAir_.temperatureC);
    result.relativeHumidityPct = static_cast<float>(
        100.0 * nearAir_.vaporKgM2 / std::max(0.000001, saturationMassKgM2(nearAir_)));
    result.pressureHpa = static_cast<float>(pressureHpa_);
    result.cloudCover = static_cast<float>(clamp01(upperAir_.liquidKgM2 / 0.35));
    result.precipitationMmPerHour = static_cast<float>(precipitationMmPerHour_);
    result.windSpeedMps = static_cast<float>(windSpeedMps_);
    result.windDirectionDegrees = static_cast<float>(windDirectionDegrees_);
    result.groundWetness = static_cast<float>(clamp01(
        (ground_.filmKgM2 + ground_.waterKgM2) / profile_.environment.surfaceFilmCapacityKgM2));
    result.snowDepthM = static_cast<float>(ground_.snowKgM2 / 100.0);
    result.waterBalanceErrorKgM2 = static_cast<float>(
        totalWaterKgM2() - initialWaterKgM2_ + drainedWaterKgM2_ + exportedWaterKgM2_ - importedWaterKgM2_);
    applyEnvironmentGameplay(profile_.environment,result);
    return result;
}


}
