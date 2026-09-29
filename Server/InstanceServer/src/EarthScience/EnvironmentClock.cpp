#include "../InstanceWeather.h"
#include "WeatherMath.h"
namespace heaven::instance {
void InstanceWeather::updateClock() {
    const auto& p=profile_.environment;
    environment_.dayFraction=earth::cycle(simulationTimeSeconds_/p.daySeconds+p.startHour/24.0);
    environment_.yearFraction=earth::cycle(simulationTimeSeconds_/(p.daySeconds*p.yearDays)+p.startYearFraction);
    const double latitude=p.latitudeDegrees*earth::Pi/180;
    // 공전 주기에 따른 태양 적위와 자전 시각으로 태양 방향을 얻는다.
    const double declination=23.44*earth::Pi/180*std::sin(2*earth::Pi*(environment_.yearFraction-.218));
    const double hourAngle=2*earth::Pi*(environment_.dayFraction-.5);
    const double elevation=std::asin(std::clamp(std::sin(latitude)*std::sin(declination)+
        std::cos(latitude)*std::cos(declination)*std::cos(hourAngle),-1.0,1.0));
    environment_.sunElevationDegrees=elevation*180/earth::Pi;
    environment_.sunAzimuthDegrees=std::fmod(180+std::atan2(std::sin(hourAngle),
        std::cos(hourAngle)*std::sin(latitude)-std::tan(declination)*std::cos(latitude))*180/earth::Pi+360,360);
    environment_.timeScale=profile_.gameSecondsPerRealSecond;
}
}
