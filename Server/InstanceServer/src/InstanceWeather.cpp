#include "InstanceWeather.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace heaven::instance {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSimulationStepSeconds = 10.0;
constexpr double kNearAirDepthM = 120.0;
constexpr double kUpperAirDepthM = 500.0;
constexpr double kSoilCapacityKgM2 = 20.0;
constexpr double kFieldCapacityKgM2 = 12.0;
constexpr double kCloudRainThresholdKgM2 = 0.05;

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double relax(double value, double target, double dt, double responseSeconds) {
    const double alpha = -std::expm1(-dt / responseSeconds);
    return value + (target - value) * alpha;
}

// FAO 식. 온도에서 공기가 가질 수 있는 최대 수증기압(kPa)을 구한다.
double saturationPressureKPa(double temperatureC) {
    const double safeTemperature = std::clamp(temperatureC, -20.0, 50.0);
    return 0.6108 * std::exp(17.27 * safeTemperature / (safeTemperature + 237.3));
}

// 이상기체 관계로 수증기 kg/m²를 증기압 kPa로 바꾼다.
template <typename Air>
double vaporPressureKPa(const Air &air) {
    return air.vaporKgM2 * 461.5 * (air.temperatureC + 273.15) /
           (air.depthM * 1000.0);
}

template <typename Air>
double saturationMassKgM2(const Air &air) {
    return saturationPressureKPa(air.temperatureC) * 1000.0 * air.depthM /
           (461.5 * (air.temperatureC + 273.15));
}

double transfer(double &from, double &to, double requested) {
    const double moved = std::min(from, std::max(0.0, requested));
    from -= moved;
    to += moved;
    return moved;
}

template <typename Air>
void adjustSaturation(Air &air) {
    const double capacity = saturationMassKgM2(air);
    if (air.vaporKgM2 > capacity) {
        transfer(air.vaporKgM2, air.liquidKgM2, air.vaporKgM2 - capacity);
    } else {
        transfer(air.liquidKgM2, air.vaporKgM2, capacity - air.vaporKgM2);
    }
}

std::uint32_t weatherSeed(std::uint32_t type, std::uint32_t roomId) {
    // 순번이 비슷한 방끼리도 난수열이 비슷해지지 않게 두 값을 섞는다.
    std::uint32_t value = roomId * 0x9E3779B9u;
    value ^= type + 0x85EBCA6Bu + (value << 6u) + (value >> 2u);
    return value;
}

} // namespace

void InstanceWeather::initialize(std::uint32_t type, std::uint32_t roomId,
                                 const InstanceWeatherProfile &profile) {
    profile_ = profile;
    roomId_ = roomId;
    revision_ = 1;
    simulationTimeSeconds_ = 0.0;
    pendingSimulationSeconds_ = 0.0;
    drainedWaterKgM2_ = 0.0;

    std::mt19937 random(weatherSeed(type, roomId));
    std::uniform_real_distribution<double> humidityOffset(-12.0, 12.0);
    std::uniform_real_distribution<double> temperatureOffset(-2.0, 2.0);
    std::uniform_real_distribution<double> pressureOffset(-7.0, 7.0);
    std::uniform_real_distribution<double> phase(0.0, 2.0 * kPi);
    std::uniform_real_distribution<double> cloudWater(0.0, 0.16);

    weatherPhase_ = phase(random);
    const double initialTemperature = profile_.meanTemperatureC + temperatureOffset(random);
    const double humidity = std::clamp(profile_.initialRelativeHumidityPct + humidityOffset(random),
                                       25.0, 98.0) /
                            100.0;

    nearAir_ = {initialTemperature, kNearAirDepthM, 0.0, 0.0};
    upperAir_ = {initialTemperature - 8.0, kUpperAirDepthM, 0.0, cloudWater(random)};
    nearAir_.vaporKgM2 = saturationMassKgM2(nearAir_) * humidity;
    // 구름층은 포화 상태에서 시작한다. 포화 아래로 만들면 첫 단계에서 이미 있던
    // 구름물이 전부 재증발해, 초기 구름 편차가 화면에 도달하기도 전에 사라진다.
    upperAir_.vaporKgM2 = saturationMassKgM2(upperAir_);

    ground_ = {initialTemperature, profile_.initialSurfaceWaterKgM2,
               profile_.initialSoilWaterKgM2, 0.0, 0.0};
    pressureHpa_ = profile_.meanPressureHpa + pressureOffset(random);
    windSpeedMps_ = 1.0 + std::abs(pressureHpa_ - profile_.meanPressureHpa) * 0.25;
    windDirectionDegrees_ = std::fmod(weatherPhase_ * 180.0 / kPi, 360.0);
    precipitationMmPerHour_ = 0.0;
    initialWaterKgM2_ = totalWaterKgM2();
}

void InstanceWeather::advance(double realDeltaSeconds) {
    const double safeRealSeconds = std::clamp(realDeltaSeconds, 0.0, 5.0);
    pendingSimulationSeconds_ += safeRealSeconds *
                                 std::max(0.0, profile_.gameSecondsPerRealSecond);

    double simulated = 0.0;
    double precipitation = 0.0;
    while (pendingSimulationSeconds_ >= kSimulationStepSeconds) {
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

double InstanceWeather::simulateStep(double dt) {
    simulationTimeSeconds_ += dt;

    // 낮밤 렌더링과 무관한 느린 기단 변화다. 방마다 phase 가 달라 같은 종류의
    // 복제 던전도 동시에 똑같은 날씨가 되지 않는다.
    const double frontAngle = 2.0 * kPi * simulationTimeSeconds_ / (8.0 * 3600.0) + weatherPhase_;
    const double targetTemperature = profile_.meanTemperatureC + 3.0 * std::sin(frontAngle);
    const double targetPressure = profile_.meanPressureHpa + 9.0 * std::sin(frontAngle * 0.55 + 0.8);

    nearAir_.temperatureC = relax(nearAir_.temperatureC, targetTemperature, dt, 1800.0);
    upperAir_.temperatureC = relax(upperAir_.temperatureC, targetTemperature - 8.0, dt, 2400.0);
    ground_.temperatureC = relax(ground_.temperatureC, targetTemperature, dt, 3600.0);
    pressureHpa_ = relax(pressureHpa_, targetPressure, dt, 1200.0);

    const double pressureDifference = std::abs(targetPressure - pressureHpa_);
    windSpeedMps_ = relax(windSpeedMps_, 1.0 + pressureDifference * 0.45, dt, 900.0);
    windDirectionDegrees_ = std::fmod(
        weatherPhase_ * 180.0 / kPi + simulationTimeSeconds_ / 180.0, 360.0);

    // 1) 증발: 지표와 공기의 수증기압 차이가 클수록 물이 공기로 이동한다.
    const double vaporDeficitKPa = std::max(
        0.0, saturationPressureKPa(ground_.temperatureC) - vaporPressureKPa(nearAir_));
    const double evaporationRate = 0.00002 * vaporDeficitKPa;
    if (ground_.waterKgM2 > 0.0) {
        transfer(ground_.waterKgM2, nearAir_.vaporKgM2, evaporationRate * dt);
    } else {
        const double soilWetness = clamp01(ground_.soilKgM2 / kSoilCapacityKgM2);
        transfer(ground_.soilKgM2, nearAir_.vaporKgM2,
                 evaporationRate * soilWetness * 0.2 * dt);
    }

    // 2) 연직 혼합: 두 대기층의 수증기 농도 차이를 서서히 줄인다.
    const double nearConcentration = nearAir_.vaporKgM2 / nearAir_.depthM;
    const double upperConcentration = upperAir_.vaporKgM2 / upperAir_.depthM;
    const double exchangeCapacity = 1.0 / (1.0 / nearAir_.depthM + 1.0 / upperAir_.depthM);
    const double exchangeFraction = -std::expm1(-0.002 * dt / exchangeCapacity);
    const double exchanged = (nearConcentration - upperConcentration) * exchangeCapacity *
                             exchangeFraction;
    if (exchanged >= 0.0) {
        transfer(nearAir_.vaporKgM2, upperAir_.vaporKgM2, exchanged);
    } else {
        transfer(upperAir_.vaporKgM2, nearAir_.vaporKgM2, -exchanged);
    }

    // 3) 응결: 포화량을 넘긴 수증기를 구름물로 바꾼다.
    adjustSaturation(nearAir_);
    adjustSaturation(upperAir_);

    // 4) 강수: 상층 구름물이 임계량을 넘으면 비 또는 눈으로 내려온다.
    const double cloudExcess = std::max(0.0, upperAir_.liquidKgM2 - kCloudRainThresholdKgM2);
    const double falling = cloudExcess * (-std::expm1(-dt / 600.0));
    const double snowFraction = clamp01((1.0 - nearAir_.temperatureC) / 2.0);
    const double snowfall = transfer(upperAir_.liquidKgM2, ground_.snowKgM2,
                                     falling * snowFraction);
    const double rainfall = transfer(upperAir_.liquidKgM2, ground_.waterKgM2,
                                     falling * (1.0 - snowFraction));

    // 저층 액체 물방울은 안개 침착처럼 매우 천천히 지표로 내려온다.
    transfer(nearAir_.liquidKgM2, ground_.waterKgM2,
             nearAir_.liquidKgM2 * (-std::expm1(-dt / 3600.0)));

    // 5) 눈·얼음: 지표가 영상이면 녹고 영하면 지표수가 언다.
    if (ground_.temperatureC > 0.0) {
        double meltBudget = 0.00002 * ground_.temperatureC * dt;
        meltBudget -= transfer(ground_.snowKgM2, ground_.waterKgM2, meltBudget);
        transfer(ground_.iceKgM2, ground_.waterKgM2, meltBudget);
    } else {
        transfer(ground_.waterKgM2, ground_.iceKgM2,
                 0.00001 * -ground_.temperatureC * dt);
    }

    // 6) 토양: 지표수가 빈 토양으로 스며들고, 포장용수량을 넘긴 물은 배수된다.
    const double soilSpace = std::max(0.0, kSoilCapacityKgM2 - ground_.soilKgM2);
    transfer(ground_.waterKgM2, ground_.soilKgM2,
             std::min(soilSpace, 0.0001 * dt));
    const double soilExcess = std::max(0.0, ground_.soilKgM2 - kFieldCapacityKgM2);
    const double drained = soilExcess * (-std::expm1(-dt / 86400.0));
    ground_.soilKgM2 -= drained;
    drainedWaterKgM2_ += drained;
    return rainfall + snowfall;
}

double InstanceWeather::totalWaterKgM2() const {
    return nearAir_.vaporKgM2 + nearAir_.liquidKgM2 + upperAir_.vaporKgM2 +
           upperAir_.liquidKgM2 + ground_.waterKgM2 + ground_.soilKgM2 +
           ground_.snowKgM2 + ground_.iceKgM2;
}

InstanceWeatherSnapshot InstanceWeather::snapshot() const {
    InstanceWeatherSnapshot result;
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
        (ground_.waterKgM2 + ground_.soilKgM2) /
        (1.0 + kSoilCapacityKgM2)));
    result.snowDepthM = static_cast<float>(ground_.snowKgM2 / 100.0);
    result.waterBalanceErrorKgM2 = static_cast<float>(
        totalWaterKgM2() - initialWaterKgM2_ + drainedWaterKgM2_);
    return result;
}

} // namespace heaven::instance
