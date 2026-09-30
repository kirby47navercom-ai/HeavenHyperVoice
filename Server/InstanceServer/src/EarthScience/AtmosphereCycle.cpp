#include "../InstanceWeather.h"
#include "WeatherMath.h"
namespace heaven::instance {
void InstanceWeather::updateAtmosphere(double dt) {
    using namespace earth;
    const auto& p=profile_.environment;
    const double front=2*Pi*simulationTimeSeconds_/(8*3600)+weatherPhase_;
    // 남반구에서는 여름과 겨울이 반대다. 적도는 프로필에서 계절 진폭을 작게 설정한다.
    const double hemisphere=p.latitudeDegrees<0 ? -1.0 : 1.0;
    const double seasonal=-std::cos(2*Pi*environment_.yearFraction)*p.seasonalAmplitudeC*hemisphere;
    const double target=profile_.meanTemperatureC+seasonal-p.altitudeM*.0065+3*std::sin(front);
    // 지역의 기단 기온이 먼저 변하고, 지표는 별도 에너지 수지로 반응해요.
    nearAir_.temperatureC=relax(nearAir_.temperatureC,target,dt,1800);
    upperAir_.temperatureC=relax(upperAir_.temperatureC,nearAir_.temperatureC-8,dt,2400);
    const double targetPressure=profile_.meanPressureHpa+9*std::sin(front*.55+.8);
    pressureHpa_=relax(pressureHpa_,targetPressure,dt,1200);
    // 주변 공간의 기압장을 푸는 CFD가 아니다. 기단 변화와 설정된 돌풍을 합친 방 평균 풍속이다.
    const double gust=(1+std::sin(front*2.7))*.5*p.gustAmplitudeMps;
    windSpeedMps_=relax(windSpeedMps_,p.baseWindMps+gust+std::abs(targetPressure-pressureHpa_)*.45,dt,900);
    windDirectionDegrees_=std::fmod(weatherPhase_*180/Pi+simulationTimeSeconds_/180,360);
}
}
