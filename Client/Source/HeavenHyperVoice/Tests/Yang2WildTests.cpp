// YANG2_CLIENT_AUTHORITY_ONLY: 서버/게임 월드 없이 실제 Lua AI와 충돌 모델을 검사해요.
#if WITH_DEV_AUTOMATION_TESTS
#include "../Yang2/UEYang2WildSimulation.h"
#include "TriangleWorld.h"
#include "PokemonSpecies.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include <fstream>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYang2WildTest,"Heaven.Weather.Yang2WildAI",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FYang2WildTest::RunTest(const FString&) {
    const FString Binary=FPaths::GetPath(FModuleManager::Get().GetModuleFilename(TEXT("HeavenHyperVoice")));
    if(!TestNotNull(TEXT("Staged Lua DLL"),FPlatformProcess::GetDllHandle(*(Binary/TEXT("lua.dll"))))) return false;
    const auto* Base=heaven::proto::findSpeciesByDex(393);
    if(!TestNotNull(TEXT("Existing Pokemon species"),Base)) return false;
    const FString File=FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(),TEXT("Yang2AI_"),TEXT(".hhvcollision"));
    hhv::movement::TriangleWorld Terrain;
    Terrain.build({{{-2000,-2000,0},{2000,-2000,0},{2000,2000,0}},{{-2000,-2000,0},{2000,2000,0},{-2000,2000,0}}});
    {std::ofstream Out(TCHAR_TO_UTF8(*File));if(!Terrain.save(Out)) {AddError(TEXT("Collision fixture write failed"));return false;}}
    try {
        heaven::instance::InstanceWeatherProfile Profile;Profile.environment.wildRespawnSeconds=1;
        Profile.spawnRules.push_back({393,1,1,1,0}); // 밤에는 생성 불가: 리스폰 당시 날씨를 확인해요.
        heaven::instance::InstanceWeather Weather;Weather.initialize(1,1,Profile);
        auto Climate=Weather.snapshot();Climate.environment.sunElevationDegrees=30;
        FYang2WildSimulation Model(TCHAR_TO_UTF8(*File),TCHAR_TO_UTF8(*(Binary/TEXT("Yang2AI/wild_ai.lua"))),
            Terrain.hash(),1,{0,0,88.1f},100,1,Profile,{Base->id});
        TArray<FHHVFieldEntity> Spawned,Moved;TArray<uint64> Gone;
        heaven::instance::ObservedPlayer Observer{1,1,600,0,88.1f};
        auto Step=[&]() {Spawned.Reset();Moved.Reset();Gone.Reset();Model.Step(.05f,Observer,Climate,Spawned,Moved,Gone);};
        Step();if(!TestEqual(TEXT("Initial local spawn"),Spawned.Num(),1)) {IFileManager::Get().Delete(*File);return false;}
        const auto Id=Spawned[0].EntityId;auto Entity=Spawned[0];const auto Start=Entity.CoreState.position;
        TestEqual(TEXT("Renderer receives dex"),Entity.Species,uint16(393));
        for(int i=0;i<60;++i) {Step();if(!Moved.IsEmpty()) Entity=Moved[0];}
        TestTrue(TEXT("Server chase moves through canonical collision"),hhv::movement::length(Entity.CoreState.position-Start)>25);
        Observer.x=Entity.CoreState.position.x+30;Observer.y=Entity.CoreState.position.y;
        for(int i=0;i<30;++i) {Step();if(!Moved.IsEmpty()) Entity=Moved[0];}
        TestTrue(TEXT("Lua attack reaches render signal"),Entity.AttackSequence>0 && Entity.AttackTargetId==1);
        TestTrue(TEXT("Local health authority"),Model.SetHealth(Id,0));
        Climate.environment.sunElevationDegrees=-30;Step();
        TestTrue(TEXT("Dead wild despawned"),Gone.Contains(Id) && Model.AliveCount()==0);
        for(int i=0;i<30;++i) Step();
        TestEqual(TEXT("Night spawn weight prevents respawn"),Model.AliveCount(),std::size_t(0));
        Climate.environment.sunElevationDegrees=30;
        for(int i=0;i<25;++i) Step();
        TestEqual(TEXT("New weather permits respawn"),Model.AliveCount(),std::size_t(1));
    } catch(const std::exception& Error) {AddError(UTF8_TO_TCHAR(Error.what()));}
    IFileManager::Get().Delete(*File);return true;
}
#endif
