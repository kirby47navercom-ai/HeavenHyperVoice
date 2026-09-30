#include "../InstanceWeather.h"
#include "WeatherMath.h"
namespace heaven::instance {
void InstanceWeather::exchangeBoundaryMoisture(double dt) {
    using namespace earth;
    // 방은 밀폐된 병이 아니에요. 주변 기단의 수분이 들어오거나 빠져나가는 양을 따로 기록해요.
    // ponytail: 공간 이류 대신 방 평균 교환을 사용해요. 실제 국지 날씨가 필요할 때 셀 이류로 교체해요.
    const double humidity=clamp01(profile_.initialRelativeHumidityPct/100);
    for(auto* air:{&nearAir_,&upperAir_}) {
        const double target=saturationMassKgM2(*air)*humidity;
        const double next=relax(air->vaporKgM2,target,dt,profile_.environment.moistureExchangeSeconds);
        const double exchanged=next-air->vaporKgM2;
        if(exchanged>=0) importedWaterKgM2_+=exchanged;
        else exportedWaterKgM2_-=exchanged;
        air->vaporKgM2=next;
    }
}
}
