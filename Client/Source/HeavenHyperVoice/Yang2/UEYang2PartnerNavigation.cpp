// YANG2_CLIENT_AUTHORITY_ONLY: 서버의 PartnerFollower를 단독 플레이에서 실행해요. main 병합 금지.
#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "Offline partner navigation must not compile on main."
#endif
#include "../Server/UEFieldPartnerSyncComponent.h"
#include "../Character/UEPlayerCharacter.h"
#include "../Pokemon/UEPokemonCharacter.h"
#include "../Movement/UECoreCollisionSubsystem.h"
#include "PokemonSpecies.h"
#include "Async/Async.h"
#include "Misc/Paths.h"
#include <stdexcept>

void UUEFieldPartnerSyncComponent::PrepareLocalNavigation(AUEPlayerCharacter *Player)
{
    if (bLocalNavigationStarted || !Player || !GetWorld()) return;
    auto *Collision = GetWorld()->GetSubsystem<UUECoreCollisionSubsystem>();
    if (!Collision || !Collision->EnsureReady(Player->GetCoreMovement()->CollisionFile.FilePath)) return;
    const uint64 Hash = Collision->GetCollision()->hash();
    FString File = Collision->LoadedFile;
    if (File.IsEmpty()) {
        File = FPaths::ProjectSavedDir() / TEXT("Yang2") /
            FString::Printf(TEXT("Collision_%llu.hhvcollision"), Hash);
        if (!Collision->SaveCollisionFile(File)) return;
    }
    const std::string MapPath(TCHAR_TO_UTF8(*File));
    bLocalNavigationStarted = true;
    // 큰 맵의 Recast 생성은 한 번만 백그라운드에서 해요. 작업 스레드에는 UObject를 보내지 않아요.
    LocalNavigationLoading = Async(EAsyncExecution::ThreadPool, [MapPath, Hash]() -> std::shared_ptr<heaven::Map> {
        try {
            auto Map = std::make_shared<heaven::Map>(0);
            std::string Error;
            if (!Map->loadFromFile(MapPath, Error)) throw std::runtime_error(Error);
            if (Map->collision().hash() != Hash) throw std::runtime_error("Offline AI/player collision hash mismatch");
            return Map;
        } catch (const std::exception &Error) {
            UE_LOG(LogTemp, Error, TEXT("Yang2 partner navigation failed: %s"), UTF8_TO_TCHAR(Error.what()));
            return {};
        }
    });
    AddTickPrerequisiteComponent(Player->GetCoreMovement());
    SetComponentTickEnabled(true);
    UE_LOG(LogTemp, Display, TEXT("Yang2 shared partner/wild navigation is loading"));
}

void UUEFieldPartnerSyncComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction *TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (LocalNavigationLoading.IsValid() && LocalNavigationLoading.IsReady()) {
        LocalNavigation = LocalNavigationLoading.Get();
        LocalNavigationLoading = {};
        if (LocalNavigation) UE_LOG(LogTemp, Display, TEXT("Yang2 shared partner/wild navigation ready"));
    }
    if (!LocalNavigation) return;
    // 서버와 같은 20Hz 추종이에요. 프레임 지연 뒤 과도하게 한꺼번에 따라잡지 않아요.
    FollowAccumulator = FMath::Min(.25, FollowAccumulator + FMath::Max(0.f, DeltaTime));
    heaven::fieldshared::PartnerFollowConfig Config;
    Config.followForwardOffset = FMath::Max(0.f, FollowForwardOffset);
    Config.followSideOffset = FMath::Max(0.f, FollowSideOffset);
    Config.teleportDistance = FMath::Max(1.f, TeleportDistance);
    while (FollowAccumulator >= .05) {
        FollowAccumulator -= .05;
        FollowTime += .05;
        for (auto &Pair : Partners) {
            auto &Partner = Pair.Value;
            auto *Owner = Partner.LocalOwner.Get();
            auto *Actor = Partner.Actor.Get();
            const auto *Species = heaven::proto::findSpeciesByDex(Partner.DexNumber);
            if (!Owner || !Actor || !Species) continue;
            const FVector Position = Owner->GetActorLocation(), Velocity = Owner->GetVelocity();
            const heaven::fieldshared::PartnerOwnerState OwnerState{
                {float(Position.X), float(Position.Y), float(Position.Z)},
                {float(Velocity.X), float(Velocity.Y), float(Velocity.Z)}, float(Owner->GetActorRotation().Yaw)};
            heaven::fieldshared::PartnerFollower::update(.05f, OwnerState, Species->id,
                Partner.FollowState, LocalNavigation.get(), Config);
            if (!Partner.FollowState.initialized) continue;
            Actor->SetActorHiddenInGame(false);
            // 위치뿐 아니라 속도/회전을 보내 기존 AnimBP의 Idle/Walk/Run을 그대로 작동시켜요.
            ApplyPartnerServerState(Pair.Key, Partner.FollowState.movement,
                Partner.FollowState.teleportedThisTick, FollowTime);
        }
    }
}

void UUEFieldPartnerSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DestroyPartners();
    Super::EndPlay(EndPlayReason);
}
