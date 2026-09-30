// YANG2_CLIENT_AUTHORITY_ONLY
#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "Local world clock must not compile on main."
#endif
#include "UEYang2WorldClock.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "../../../../Server/InstanceServer/src/EarthScience/WorldEnvironmentClock.h"
#include "../../../../Server/InstanceServer/src/EarthScience/EnvironmentCheckpoint.h"
#include "Misc/Paths.h"
#include <iomanip>
#include <sstream>
#include <map>
#include <stdexcept>
namespace {
struct FSessionClock {
    heaven::instance::WorldEnvironmentClock Clock;
    double Scale=0,Day=0,Year=0,Hour=0,Season=0;
    double LastSave=0;
    std::map<uint32,std::string> Climates;
    bool Loaded=false,CanSave=true;
};
std::filesystem::path SaveFile() {return std::filesystem::path(*(FPaths::ProjectSavedDir()/TEXT("Yang2/environment.state")));}
void LoadSession(FSessionClock& State,const std::filesystem::path& File) {
    if(State.Loaded) return;State.Loaded=true;
    try {
        auto Saved=heaven::instance::readEnvironmentCheckpoint(File);
        if(!Saved) {
            if(std::filesystem::exists(File) || std::filesystem::exists(File.wstring()+L".bak")) throw std::runtime_error("Local environment saves are damaged");
            return;
        }
        FSessionClock Next;std::istringstream In(*Saved);std::string Magic;double Seconds=0;std::size_t Count=0;
        if(!(In>>Magic>>Seconds>>Next.Scale>>Next.Day>>Next.Year>>Next.Hour>>Next.Season>>Count) || Magic!="YANG2ENV1" || Count>128 || !std::isfinite(Seconds) || Seconds<0 || Seconds>1e12)
            throw std::runtime_error("Invalid local environment header");
        for(double V:{Next.Scale,Next.Day,Next.Year,Next.Hour,Next.Season}) if(!std::isfinite(V) || V<0) throw std::runtime_error("Invalid local clock");
        for(std::size_t i=0;i<Count;++i) {uint32 Id=0;std::string Climate;
            if(!(In>>Id>>std::quoted(Climate)) || Id==0 || !Next.Climates.emplace(Id,Climate).second) throw std::runtime_error("Invalid local climate");}
        std::string Extra;if(In>>Extra) throw std::runtime_error("Unexpected local save data");
        State.Scale=Next.Scale;State.Day=Next.Day;State.Year=Next.Year;State.Hour=Next.Hour;State.Season=Next.Season;
        State.Climates=std::move(Next.Climates);State.Clock.restore(Seconds);State.LastSave=Seconds;
    } catch(const std::exception& Error) {
        State.CanSave=false;UE_LOG(LogTemp,Error,TEXT("Yang2 environment restore failed; original save preserved: %s"),UTF8_TO_TCHAR(Error.what()));
    }
}
FSessionClock* Session(UWorld* World) {
    if(!World || !World->GetGameInstance()) return nullptr;
    static TMap<TWeakObjectPtr<UGameInstance>,FSessionClock> Sessions;
    for(auto It=Sessions.CreateIterator();It;++It) if(!It.Key().IsValid()) It.RemoveCurrent();
    auto& State=Sessions.FindOrAdd(World->GetGameInstance());LoadSession(State,SaveFile());return &State;
}
}
bool RestoreYang2Climate(UWorld* World,heaven::instance::InstanceWeather& Weather) {
    auto* State=Session(World);if(!State) return false;
    auto It=State->Climates.find(Weather.snapshot().roomId);if(It==State->Climates.end()) return false;
    std::istringstream In(It->second);
    if(!Weather.restore(In)) {
        State->CanSave=false;
        UE_LOG(LogTemp,Error,TEXT("Yang2 saved climate is incompatible with current profile; save preserved."));return false;
    }
    Weather.synchronizeClock(State->Clock.elapsedRealSeconds());return true;
}
void SaveYang2Environment(UWorld* World,const heaven::instance::InstanceWeather* Weather,bool Force) {
    auto* State=Session(World);if(!State || !State->CanSave) return;
    const double Seconds=State->Clock.elapsedRealSeconds();
    if(!Force && Seconds-State->LastSave<30) return;
    if(Weather) {std::ostringstream Climate;Weather->save(Climate);State->Climates[Weather->snapshot().roomId]=Climate.str();}
    if(State->Climates.size()>128) {UE_LOG(LogTemp,Error,TEXT("Yang2 environment room limit exceeded"));return;}
    std::ostringstream Out;Out<<std::setprecision(17)<<"YANG2ENV1 "<<Seconds<<' '<<State->Scale<<' '<<State->Day<<' '<<State->Year<<' '<<State->Hour<<' '<<State->Season<<' '<<State->Climates.size()<<'\n';
    for(const auto& [Id,Climate]:State->Climates) Out<<Id<<' '<<std::quoted(Climate)<<'\n';
    // ponytail: 로컬 시험의 작은 파일만 게임 스레드에서 저장해요. 대규모 방은 비동기 저장으로 바꾸세요.
    try {heaven::instance::writeEnvironmentCheckpoint(SaveFile(),Out.str());State->LastSave=Seconds;}
    catch(const std::exception& Error){UE_LOG(LogTemp,Error,TEXT("Yang2 environment save failed: %s"),UTF8_TO_TCHAR(Error.what()));}
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

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYang2SaveTest,"Heaven.Weather.Yang2Persistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FYang2SaveTest::RunTest(const FString&) {
    // 실제 플레이 저장은 건드리지 않고 같은 로컬 복구 함수를 임시 파일로 실행해요.
    const FString Name=FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(),TEXT("Yang2Save_"),TEXT(".state"));
    const std::filesystem::path File(*Name);
    heaven::instance::InstanceWeather Model;Model.initialize(1,123,{});Model.advance(20);
    std::ostringstream Climate;Model.save(Climate);
    std::ostringstream Out;Out<<"YANG2ENV1 12 60 86400 120 3 0.25 1\n123 "<<std::quoted(Climate.str())<<'\n';
    heaven::instance::writeEnvironmentCheckpoint(File,Out.str());
    FSessionClock Restored;LoadSession(Restored,File);
    TestTrue(TEXT("Local clock restored"),Restored.Clock.elapsedRealSeconds()>=12 && Restored.Clock.elapsedRealSeconds()<13);
    TestEqual(TEXT("Shared clock settings restored"),Restored.Hour,3.);
    TestTrue(TEXT("Complete climate restored"),Restored.Climates[123]==Climate.str());
    heaven::instance::InstanceWeather Back;Back.initialize(1,123,{});std::istringstream In(Restored.Climates[123]);
    TestTrue(TEXT("Saved local climate accepted by native model"),Back.restore(In));
    TestEqual(TEXT("Restored local temperature"),Back.snapshot().temperatureC,Model.snapshot().temperatureC);
    heaven::instance::writeEnvironmentCheckpoint(File,"YANG2ENV1 invalid");
    AddExpectedError(TEXT("Yang2 environment restore failed"),EAutomationExpectedErrorFlags::Contains,1);
    FSessionClock Broken;LoadSession(Broken,File);
    TestFalse(TEXT("Invalid semantic save never overwritten"),Broken.CanSave);
    IFileManager::Get().Delete(*Name);IFileManager::Get().Delete(*(Name+TEXT(".bak")));
    return true;
}
#endif
