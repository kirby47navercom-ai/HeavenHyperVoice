#pragma once
// YANG2_CLIENT_AUTHORITY_ONLY: 이 어댑터는 main에 합치면 안 된다.
#include "UEYang2InstanceWeather.h"
#include "../Environment/UEEnvironmentState.h"
class UWorld;
class UUEEnvironmentProfile;
void ApplyYang2EnvironmentProfile(UWorld* World,const UUEEnvironmentProfile* Override,
    heaven::instance::InstanceWeatherProfile& Result);
FUEEnvironmentState MakeYang2EnvironmentState(const heaven::instance::EnvironmentState& Source);
