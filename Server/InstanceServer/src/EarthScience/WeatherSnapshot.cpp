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
    const auto fuzzy = evaluateFuzzyWeather(profile_.environment.fuzzyWeather, fuzzyWeatherInputs());
    result.environment.fuzzyWeatherEnabled = true;
    result.environment.snowFraction = fuzzy.snowFraction;
    result.environment.fogDensity = fuzzy.fogDensity;
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

// 현재 방의 값을 퍼지 입력으로 모아요. 맵 주소나 플레이어 좌표에 의존하지 않아요.
FuzzyWeatherInputs InstanceWeather::fuzzyWeatherInputs() const {
    const auto& p = profile_.environment;
    FuzzyWeatherInputs inputs;
    inputs.temperatureC = nearAir_.temperatureC;
    inputs.upperHumidityPct = 100 * upperAir_.vaporKgM2 / std::max(.000001, saturationMassKgM2(upperAir_));
    inputs.nearHumidityPct = 100 * nearAir_.vaporKgM2 / std::max(.000001, saturationMassKgM2(nearAir_));
    inputs.pressureDeficitHpa = profile_.meanPressureHpa - pressureHpa_;
    inputs.cloudCover = clamp01(upperAir_.liquidKgM2 / .35);
    inputs.surfaceCoolingC = nearAir_.temperatureC - ground_.temperatureC;
    inputs.windSpeedMps = windSpeedMps_;
    inputs.soilDryness = 1 - clamp01((ground_.waterKgM2 + ground_.soilKgM2) / p.soilCapacityKgM2);
    inputs.surfaceDryness = 1 - clamp01((ground_.waterKgM2 + ground_.filmKgM2) / p.surfaceFilmCapacityKgM2);
    inputs.dustWind = clamp01((windSpeedMps_ - p.dustStartWindMps) / (p.dustFullWindMps - p.dustStartWindMps));
    return inputs;
}


}
