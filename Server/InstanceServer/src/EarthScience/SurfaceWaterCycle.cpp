#include "../InstanceWeather.h"
#include "WeatherMath.h"
namespace heaven::instance {
using namespace earth;
// 대기와 지표 저장소 사이의 물 이동. 조석/렌더링과 분리해 물 보존을 유지한다.
double InstanceWeather::updateHydrology(double dt) {
    // 1) 증발: 지표와 공기의 수증기압 차이가 클수록 물이 공기로 이동한다.
    const double vaporDeficitKPa = std::max(
        0.0, saturationPressureKPa(ground_.temperatureC) - vaporPressureKPa(nearAir_));
    const double evaporationRate = 0.00002 * vaporDeficitKPa;
    double evaporated=transfer(ground_.filmKgM2,nearAir_.vaporKgM2,evaporationRate*dt);
    const double evaporationBudget=std::max(0.0,evaporationRate*dt-evaporated);
    if (ground_.waterKgM2 > 0.0) {
        evaporated+=transfer(ground_.waterKgM2, nearAir_.vaporKgM2, evaporationBudget);
    } else {
        const double soilWetness = clamp01(ground_.soilKgM2 / profile_.environment.soilCapacityKgM2);
        evaporated+=transfer(ground_.soilKgM2, nearAir_.vaporKgM2,
                 evaporationBudget * soilWetness * 0.2);
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
    const double cloudExcess = std::max(0.0, upperAir_.liquidKgM2 - 0.05);
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
    double melted=0;
    if (ground_.temperatureC > 0.0) {
        double meltBudget = 0.00002 * ground_.temperatureC * dt;
        melted=transfer(ground_.snowKgM2, ground_.waterKgM2, meltBudget);
        melted+=transfer(ground_.iceKgM2, ground_.waterKgM2, meltBudget-melted);
    } else {
        double freezeBudget=0.00001 * -ground_.temperatureC * dt;
        const double filmFrozen=transfer(ground_.filmKgM2,ground_.iceKgM2,freezeBudget);
        melted=-filmFrozen-transfer(ground_.waterKgM2,ground_.iceKgM2,freezeBudget-filmFrozen);
    }

    transfer(ground_.waterKgM2,ground_.filmKgM2,
        std::max(0.0,profile_.environment.surfaceFilmCapacityKgM2-ground_.filmKgM2));
    applyLatentHeat(evaporated,melted);

    // 6) 토양: 지표수가 빈 토양으로 스며들고, 포장용수량을 넘긴 물은 배수된다.
    const double soilSpace = std::max(0.0, profile_.environment.soilCapacityKgM2 - ground_.soilKgM2);
    transfer(ground_.waterKgM2, ground_.soilKgM2,
             std::min(soilSpace, profile_.environment.infiltrationKgM2PerSecond * dt));
    const double soilExcess = std::max(0.0, ground_.soilKgM2 - profile_.environment.fieldCapacityKgM2);
    const double drained = soilExcess * (-std::expm1(-dt / profile_.environment.drainageSeconds));
    ground_.soilKgM2 -= drained;
    drainedWaterKgM2_ += drained;
    return rainfall + snowfall;
}

}
