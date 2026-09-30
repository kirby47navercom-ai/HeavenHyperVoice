// YANG2_CLIENT_AUTHORITY_ONLY
#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "Local world clock must not compile on main."
#endif
#include "UEYang2WorldClock.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "../../../../Server/InstanceServer/src/EarthScience/WorldEnvironmentClock.h"
namespace {
struct FSessionClock {
    heaven::instance::WorldEnvironmentClock Clock;
    double Scale=0,Day=0,Year=0,Hour=0,Season=0;
};
FSessionClock* Session(UWorld* World) {
    if(!World || !World->GetGameInstance()) return nullptr;
    static TMap<TWeakObjectPtr<UGameInstance>,FSessionClock> Sessions;
    for(auto It=Sessions.CreateIterator();It;++It) if(!It.Key().IsValid()) It.RemoveCurrent();
    return &Sessions.FindOrAdd(World->GetGameInstance());
}
}
double GetYang2WorldRealSeconds(UWorld* World) {
    const auto* State=Session(World); return State ? State->Clock.elapsedRealSeconds() : 0;
}
void ApplyYang2WorldClockSettings(UWorld* World,heaven::instance::InstanceWeatherProfile& Profile) {
    auto* State=Session(World); if(!State) return;
    auto& P=Profile.environment;
    if(State->Day==0) {
        State->Scale=Profile.gameSecondsPerRealSecond;
        State->Day=P.daySeconds; State->Year=P.yearDays; State->Hour=P.startHour; State->Season=P.startYearFraction;
    }
    // 맵을 바꿔도 첫 인스턴스의 세계 시계 설정을 유지해요. 지역 기후 값은 각 DA 그대로예요.
    Profile.gameSecondsPerRealSecond=State->Scale;
    P.daySeconds=State->Day; P.yearDays=State->Year; P.startHour=State->Hour; P.startYearFraction=State->Season;
}
