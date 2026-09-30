#include "InstanceWeather.h"
#include "EarthScience/WeatherMath.h"
#include <random>
namespace heaven::instance {
using namespace earth;
namespace {
constexpr double kSimulationStepSeconds=10;
constexpr double kNearAirDepthM=120;
constexpr double kUpperAirDepthM=500;
constexpr double kPi=earth::Pi;
std::uint32_t weatherSeed(std::uint32_t type,std::uint32_t roomId) {
    std::uint32_t value=roomId*0x9E3779B9u;
    return value^(type+0x85EBCA6Bu+(value<<6u)+(value>>2u));
}
}
void InstanceWeather::initialize(std::uint32_t type, std::uint32_t roomId,
                                 const InstanceWeatherProfile &profile, double worldRealSeconds) {
    profile_ = profile;
    environment_ = {};
    normalizeEnvironment(profile_.environment);
    roomId_ = roomId;
    revision_ = 1;
    simulationTimeSeconds_ = 0.0;
    pendingSimulationSeconds_ = 0.0;
    drainedWaterKgM2_ = 0.0;
    importedWaterKgM2_=exportedWaterKgM2_=0;
    worldSimulationSeconds_=std::max(0.0,worldRealSeconds)*profile_.gameSecondsPerRealSecond;

    std::mt19937 random(weatherSeed(type, roomId));
    std::uniform_real_distribution<double> humidityOffset(-12.0, 12.0);
    std::uniform_real_distribution<double> temperatureOffset(-2.0, 2.0);
    std::uniform_real_distribution<double> pressureOffset(-7.0, 7.0);
    std::uniform_real_distribution<double> phase(0.0, 2.0 * kPi);
    std::uniform_real_distribution<double> cloudWater(0.0, 0.16);

    weatherPhase_ = phase(random);
    const double initialTemperature = profile_.meanTemperatureC + temperatureOffset(random);
    const double humidity = std::clamp(profile_.initialRelativeHumidityPct + humidityOffset(random)*
                                       std::min(profile_.initialRelativeHumidityPct,100-profile_.initialRelativeHumidityPct)/50,
                                       0.0, 100.0) /
                            100.0;

    nearAir_ = {initialTemperature, kNearAirDepthM, 0.0, 0.0};
    upperAir_ = {initialTemperature - 8.0, kUpperAirDepthM, 0.0, cloudWater(random)*clamp01((humidity-.6)/.4)};
    nearAir_.vaporKgM2 = saturationMassKgM2(nearAir_) * humidity;
    // 건조한 지역은 상층까지 강제로 포화시키지 않아요. 초기 구름이 있으면 그 층만 포화예요.
    upperAir_.vaporKgM2 = saturationMassKgM2(upperAir_)*(upperAir_.liquidKgM2>0 ? 1 : humidity);

    ground_ = {initialTemperature, profile_.initialSurfaceWaterKgM2,
               profile_.initialSoilWaterKgM2, 0.0, 0.0};
    transfer(ground_.waterKgM2,ground_.filmKgM2,profile_.environment.surfaceFilmCapacityKgM2);
    pressureHpa_ = profile_.meanPressureHpa + pressureOffset(random);
    windSpeedMps_ = 1.0 + std::abs(pressureHpa_ - profile_.meanPressureHpa) * 0.25;
    windDirectionDegrees_ = std::fmod(weatherPhase_ * 180.0 / kPi, 360.0);
    precipitationMmPerHour_ = 0.0;
    initialWaterKgM2_ = totalWaterKgM2();
    updateClock();
    updateCoast(0);
}

void InstanceWeather::advance(double realDeltaSeconds) {
    if (!std::isfinite(realDeltaSeconds) || realDeltaSeconds <= 0) return;
    const double safeRealSeconds = realDeltaSeconds;
    pendingSimulationSeconds_ += safeRealSeconds *
                                 std::max(0.0, profile_.gameSecondsPerRealSecond);

    double simulated = 0.0;
    double precipitation = 0.0;
    int steps=0;
    while (pendingSimulationSeconds_ >= kSimulationStepSeconds && steps++ < 4096) {
        precipitation += simulateStep(kSimulationStepSeconds);
        pendingSimulationSeconds_ -= kSimulationStepSeconds;
        simulated += kSimulationStepSeconds;
    }

    if (simulated > 0.0) {
        // 물 1 kg/m²는 강수량 1 mm와 같다.
        precipitationMmPerHour_ = precipitation / simulated * 3600.0;
        ++revision_;
    }
}

void InstanceWeather::synchronizeClock(double worldRealSeconds) {
    if(!std::isfinite(worldRealSeconds) || worldRealSeconds<0) return;
    worldSimulationSeconds_=worldRealSeconds*profile_.gameSecondsPerRealSecond;
    updateClock(); updateCoast(0);
}
// 각 환경의 순서만 관리한다. 자세한 계산은 EarthScience 폴더의 기능별 cpp에 있다.
double InstanceWeather::simulateStep(double dt) {
    simulationTimeSeconds_+=dt;
    worldSimulationSeconds_+=dt;
    updateClock();
    updateAtmosphere(dt);
    updateSurfaceEnergy(dt);
    exchangeBoundaryMoisture(dt);
    const double precipitation=updateHydrology(dt);
    updateCoast(dt);
    updateDesert(dt);
    return precipitation;
}
}
