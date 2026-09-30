#include "../InstanceWeather.h"
#include "WeatherMath.h"
namespace heaven::instance {
void InstanceWeather::updateSurfaceEnergy(double dt) {
    using namespace earth;
    const auto& p=profile_.environment;
    const double clouds=clamp01(upperAir_.liquidKgM2/.35);
    const double elevation=std::max(0.0,std::sin(environment_.sunElevationDegrees*Pi/180));
    // 일사 흡수: 구름과 반사율이 흡수되는 W/m²를 줄여요. dailyAmplitudeC는 지역 일사 세기 보정이에요.
    const double solar=p.solarPeakWm2*(1-p.surfaceAlbedo)*elevation*(1-.7*clouds)*(p.dailyAmplitudeC/5);
    const double surfaceK=ground_.temperatureC+273.15;
    const double airK=nearAir_.temperatureC+273.15;
    // 대기와의 장파 복사 차이 + 맑은 하늘로 빠져나가는 복사. 구름은 밤 냉각을 줄여요.
    const double radiation=p.surfaceEmissivity*5.670374419e-8*(std::pow(surfaceK,4)-std::pow(airK,4))
        +p.clearSkyCoolingWm2*(1-.85*clouds);
    const double exchange=p.airHeatTransferWm2K*(ground_.temperatureC-nearAir_.temperatureC);
    environment_.surfaceHeatFluxWm2=solar-radiation-exchange;
    // 열용량 = 열 교환 계수 × 반응 시간. 아주 작은 응답 시간도 명시적으로 안정되게 적분해요.
    const double capacity=std::max(1000.0,p.airHeatTransferWm2K*p.thermalResponseSeconds);
    const double damping=p.airHeatTransferWm2K+4*p.surfaceEmissivity*5.670374419e-8*std::pow(surfaceK,3);
    ground_.temperatureC+=environment_.surfaceHeatFluxWm2/damping*(-std::expm1(-damping*dt/capacity));
    ground_.temperatureC=std::clamp(ground_.temperatureC,-80.0,80.0);
}
void InstanceWeather::applyLatentHeat(double evaporatedKgM2,double meltedKgM2) {
    // 물의 증발/융해에 사용한 열을 지표에서 빼요. 동결량은 음수이므로 열이 되돌아와요.
    const auto& p=profile_.environment;
    const double capacity=std::max(1000.0,p.airHeatTransferWm2K*p.thermalResponseSeconds);
    ground_.temperatureC=std::clamp(ground_.temperatureC-
        (evaporatedKgM2*2450000+meltedKgM2*333550)/capacity,-80.0,80.0);
}
}
