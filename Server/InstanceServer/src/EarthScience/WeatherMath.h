#pragma once
#include <algorithm>
#include <cmath>
namespace heaven::instance::earth {
inline constexpr double Pi = 3.14159265358979323846;
inline double clamp01(double x) { return std::clamp(x,0.0,1.0); }
// dt를 여러 번 쪼개도 같은 목표에 대한 감쇠율이 유지되는 지수 보간이다.
inline double relax(double x,double target,double dt,double response) {
    return x+(target-x)*(-std::expm1(-dt/std::max(response,0.001)));
}
inline double cycle(double value) { return value-std::floor(value); }
// 물을 생성하지 않고 두 저장소 사이에서만 옮긴다.
inline double transfer(double& from,double& to,double amount) {
    const double moved=std::min(from,std::max(0.0,amount)); from-=moved; to+=moved; return moved;
}
inline double saturationPressureKPa(double temperature) {
    const double t=std::clamp(temperature,-80.0,80.0);
    // 영하에서는 얼음 위 포화 수증기압을 사용해요. 낮은 기온을 -20도로 잘라 버리지 않아요.
    return t<0 ? .6112*std::exp(22.46*t/(272.62+t)) : .6112*std::exp(17.67*t/(243.5+t));
}
template<class Air> double vaporPressureKPa(const Air& air) {
    return air.vaporKgM2*461.5*(air.temperatureC+273.15)/(air.depthM*1000.0);
}
template<class Air> double saturationMassKgM2(const Air& air) {
    return saturationPressureKPa(air.temperatureC)*1000.0*air.depthM/(461.5*(air.temperatureC+273.15));
}
template<class Air> void adjustSaturation(Air& air) {
    const double capacity=saturationMassKgM2(air);
    if(air.vaporKgM2>capacity) transfer(air.vaporKgM2,air.liquidKgM2,air.vaporKgM2-capacity);
    else transfer(air.liquidKgM2,air.vaporKgM2,capacity-air.vaporKgM2);
}
}
