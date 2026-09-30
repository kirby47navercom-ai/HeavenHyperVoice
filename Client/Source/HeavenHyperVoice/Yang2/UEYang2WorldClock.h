#pragma once
// YANG2_CLIENT_AUTHORITY_ONLY: 레벨 전환과 재실행에도 이어지는 로컬 공용 시계예요.
#include "UEYang2InstanceWeather.h"
class UWorld;
double GetYang2WorldRealSeconds(UWorld* World);
void ApplyYang2WorldClockSettings(UWorld* World,heaven::instance::InstanceWeatherProfile& Profile);
bool RestoreYang2Climate(UWorld* World,heaven::instance::InstanceWeather& Weather);
void SaveYang2Environment(UWorld* World,const heaven::instance::InstanceWeather* Weather,bool Force=false);
