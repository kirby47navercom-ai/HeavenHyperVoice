// YANG2_CLIENT_AUTHORITY_ONLY
#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "Offline wild bridge must not compile on main."
#endif
#include "../Server/UEFieldServerBridgeComponent.h"
#include "UEYang2WildSimulation.h"
#include "../Server/UEFieldWildPokemonSyncComponent.h"
#include "../Server/UEFieldPartnerSyncComponent.h"
#include "../Character/UEPlayerCharacter.h"
#include "../Movement/UECoreMovementComponent.h"
#include "PokemonSpecies.h"
#include "Async/Async.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"

void UUEFieldServerBridgeComponent::StartYang2Wild(const heaven::instance::InstanceWeatherProfile& Profile) {
    auto* Player=GetPlayerCharacter();auto* World=GetWorld();
    if(!World || !Player || !WildPokemonSyncComponent.IsValid() || Yang2WildPerRoom<=0) return;
    // 파트너와 같은 길찾기 맵을 사용해요. Recast 준비가 끝난 뒤 여기로 들어와요.
    const auto Navigation=PartnerSyncComponent.IsValid() ? PartnerSyncComponent->GetLocalNavigation() : nullptr;
    if(!Navigation) return;
    const FString Binary=FPaths::GetPath(FModuleManager::Get().GetModuleFilename(TEXT("HeavenHyperVoice")));
    // DLL은 프로세스 동안 한 번만 유지해요. 레벨 진입마다 로더 참조가 늘지 않아요.
    static void* LuaRuntime=nullptr;
    if(!LuaRuntime) LuaRuntime=FPlatformProcess::GetDllHandle(*(Binary/TEXT("lua.dll")));
    if(!LuaRuntime) {
        UE_LOG(LogTemp,Error,TEXT("Yang2 Lua runtime could not load from the module directory"));return;
    }
    const FString Script=FPaths::IsRelative(Yang2WildAiScript.FilePath) ? Binary/Yang2WildAiScript.FilePath : Yang2WildAiScript.FilePath;
    if(Yang2WildAiScript.FilePath.IsEmpty() || !FPaths::FileExists(Script)) {
        UE_LOG(LogTemp,Error,TEXT("Yang2 wild AI script is not assigned or missing: %s"),*Script);return;
    }
    std::vector<uint16> Pool;
    for(int32 Dex:Yang2WildSpeciesDex) {
        if(Dex<1 || Dex>65535 || !heaven::proto::isWildSpawnable(static_cast<uint16>(Dex))) continue;
        if(const auto* Base=heaven::proto::findSpeciesByDex(static_cast<uint16>(Dex))) Pool.push_back(Base->id);
    }
    if(!Yang2WildSpeciesDex.IsEmpty() && Pool.empty()) {UE_LOG(LogTemp,Error,TEXT("Yang2 wild species list has no valid wild dex"));return;}
    const auto Position=Player->GetActorLocation();const heaven::nav::Vec3 Center{float(Position.X),float(Position.Y),float(Position.Z)};
    const uint32 Type=TargetInstanceType;const int32 Count=Yang2WildPerRoom;const float Extent=Yang2WildAreaCm;
    const std::string ScriptPath(TCHAR_TO_UTF8(*Script));
    // Lua 준비도 게임 스레드를 멈추지 않아요. UObject는 작업 스레드로 보내지 않아요.
    Yang2WildLoading=Async(EAsyncExecution::ThreadPool,[Navigation,ScriptPath,Type,Center,Extent,Count,Profile,Pool]()->std::shared_ptr<FYang2WildSimulation> {
        try {return std::make_shared<FYang2WildSimulation>(Navigation,ScriptPath,Type,Center,Extent,Count,Profile,Pool);}
        catch(const std::exception& Error) {UE_LOG(LogTemp,Error,TEXT("Yang2 wild AI initialization failed: %s"),UTF8_TO_TCHAR(Error.what()));return {};}
    });
    UE_LOG(LogTemp,Display,TEXT("Yang2 server Lua AI is loading in the background (%d slots)"),Count);
}
void UUEFieldServerBridgeComponent::TickYang2Wild(float DeltaTime) {
    if(bYang2WildStartPending && PartnerSyncComponent.IsValid() && PartnerSyncComponent->GetLocalNavigation()) {
        bYang2WildStartPending=false;StartYang2Wild(Yang2ActiveProfile);
    }
    if(Yang2WildLoading.IsValid() && Yang2WildLoading.IsReady()) {
        Yang2Wild=Yang2WildLoading.Get();Yang2WildLoading={};
        if(Yang2Wild) UE_LOG(LogTemp,Display,TEXT("Yang2 server Lua AI ready"));
    }
    auto* Player=GetPlayerCharacter();
    if(!Yang2Wild || !Yang2LocalWeather || !Player || !WildPokemonSyncComponent.IsValid()) return;
    Yang2WildAccumulator=FMath::Min(.25,Yang2WildAccumulator+FMath::Max(0.f,DeltaTime));
    const auto Position=Player->GetActorLocation();
    const heaven::instance::ObservedPlayer Observer{LocalEntityId,TargetInstanceType,float(Position.X),float(Position.Y),float(Position.Z)};
    const auto Climate=Yang2LocalWeather->snapshot();
    while(Yang2WildAccumulator>=.05) {
        TArray<FHHVFieldEntity> Spawned,Moved;TArray<uint64> Gone;
        Yang2Wild->Step(.05f,Observer,Climate,Spawned,Moved,Gone);Yang2WildAccumulator-=.05;
        for(uint64 Id:Gone) WildPokemonSyncComponent->HandleWildPokemonDespawned(Id);
        for(const auto& Entity:Spawned) WildPokemonSyncComponent->HandleWildPokemonSpawned(Entity);
        for(const auto& Entity:Moved) WildPokemonSyncComponent->HandleWildPokemonMoved(Entity);
    }
}
bool UUEFieldServerBridgeComponent::SetYang2WildHealth(int64 EntityId,int32 HP) {
    return Yang2Wild && EntityId>0 && Yang2Wild->SetHealth(static_cast<uint64>(EntityId),static_cast<uint16>(FMath::Clamp(HP,0,65535)));
}
int32 UUEFieldServerBridgeComponent::GetYang2WildCount() const {return Yang2Wild ? static_cast<int32>(Yang2Wild->AliveCount()) : 0;}
