#pragma once
// YANG2_CLIENT_AUTHORITY_ONLY: GameInstance가 살아 있는 동안만 유지하는 로컬 공용 시계예요.
#include "UEYang2InstanceWeather.h"
class UWorld;
double GetYang2WorldRealSeconds(UWorld* World);
void ApplyYang2WorldClockSettings(UWorld* World,heaven::instance::InstanceWeatherProfile& Profile);
