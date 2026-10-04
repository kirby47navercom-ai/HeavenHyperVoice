#include "UEFieldPartnerSyncComponent.h"

#include "../Pokemon/UEPokemonCharacter.h"
#include "../Animation/UEPokemonAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

#include "Engine/World.h"

#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "YANG2_CLIENT_AUTHORITY_ONLY local partner changes must never compile on main."
#endif

UUEFieldPartnerSyncComponent::UUEFieldPartnerSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UUEFieldPartnerSyncComponent::SetPartnerPokemonClass(
    TSubclassOf<AUEPokemonCharacter> InPartnerPokemonClass)
{
	if (InPartnerPokemonClass)
	{
		PartnerPokemonClass = InPartnerPokemonClass;
	}
}

void UUEFieldPartnerSyncComponent::AddPartner(uint64 OwnerEntityId, AActor *OwnerActor, int32 DexNumber)
{
	if (DexNumber <= 0 || OwnerActor == nullptr)
	{
		return;
	}
	if (const FPartner *Existing = Partners.Find(OwnerEntityId))
	{
		if (Existing->Actor.IsValid() && Existing->DexNumber == DexNumber)
		{
			return;
		}
		RemovePartner(OwnerEntityId);
	}

	UWorld *World = GetWorld();
	if (!World)
	{
		return;
	}

	// 야생 포켓몬과 같은 액터 클래스다. 네이티브 클래스에는 메시도 종족 카탈로그도
	// 없어서, 이게 비어 있으면 스폰해 봐야 화면에 아무것도 안 보인다.
	if (!PartnerPokemonClass)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("FieldPartnerSync: PartnerPokemonClass is not assigned; partner %d will not be spawned."),
		       DexNumber);
		return;
	}

	const FVector SpawnLocation = OwnerActor->GetActorLocation();
	const FTransform SpawnTransform(OwnerActor->GetActorRotation(), SpawnLocation);

	AUEPokemonCharacter *PartnerActor =
	    World->SpawnActorDeferred<AUEPokemonCharacter>(PartnerPokemonClass, SpawnTransform, nullptr, nullptr,
	                                                   ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!PartnerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("FieldPartnerSync: partner spawn failed for owner %llu"),
		       OwnerEntityId);
		return;
	}

	PartnerActor->AutoPossessAI = EAutoPossessAI::Disabled;
	PartnerActor->AIControllerClass = nullptr;
	PartnerActor->FinishSpawning(SpawnTransform);

	// 주인과 부딪히면 서로 밀어내다 둘 다 튄다. 관통 회피는 서버 추종 로직이 맡는다.
	PartnerActor->SetActorEnableCollision(false);

	// 종족은 도감번호로 찾는다. 엔티티 id 는 서버 것이 아니라 주인 것을 그대로
	// 쓴다 — 서버에 파트너 엔티티가 없어서 겹칠 번호도 없다.
	PartnerActor->InitializeServerEntity(static_cast<int64>(OwnerEntityId), DexNumber,
	                                     EUEPokemonRenderType::Own);

	// 기본값(300)은 서버 스냅샷을 받는 야생 포켓몬 기준이다. 주인을 쫓는 파트너는
	// 주인이 달리기만 해도 그만큼 뒤처져서, 그대로 두면 매 프레임 순간이동한다.
	PartnerActor->SetServerHardSnapDistance(TeleportDistance);

	Partners.Add(OwnerEntityId, FPartner{PartnerActor, DexNumber});
}

void UUEFieldPartnerSyncComponent::AddLocalPartner(uint64 OwnerEntityId, AActor *OwnerActor,
	int32 DexNumber)
{
	// YANG2_CLIENT_AUTHORITY_ONLY
	AddPartner(OwnerEntityId, OwnerActor, DexNumber);
	FPartner* Partner = Partners.Find(OwnerEntityId);
	if (!Partner || !Partner->Actor.IsValid() || !OwnerActor)
	{
		return;
	}

	AUEPokemonCharacter* PartnerActor = Partner->Actor.Get();
	// 부착만 하면 속도가 0으로 남고 장애물도 무시해요. 서버 추종 결과를 같은 보간 경로로 보내요.
	PartnerActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	PartnerActor->SetActorHiddenInGame(true); // 길찾기 준비 전 주인과 겹친 모델을 숨겨요.
	PartnerActor->GetMesh()->AddTickPrerequisiteActor(PartnerActor);
	Partner->LocalOwner = OwnerActor;
	Partner->FollowState = {};
	SetComponentTickEnabled(true);
}

bool UUEFieldPartnerSyncComponent::PlayLocalPartnerAttack(uint64 OwnerEntityId, int32 AttackSlot)
{
	// YANG2_CLIENT_AUTHORITY_ONLY
	const FPartner* Partner = Partners.Find(OwnerEntityId);
	if (!Partner || !Partner->Actor.IsValid() || Partner->Actor->IsHidden() || AttackSlot < 1 || AttackSlot > 4)
	{
		return false;
	}
	auto *Animation = Cast<UUEPokemonAnimInstance>(Partner->Actor->GetMesh()->GetAnimInstance());
	// 네 슬롯을 모두 Attack01로 보내던 임시 경로를 실제 공격 종류로 나눠요.
	const EUEPokemonAttackAnimation Attacks[] = {EUEPokemonAttackAnimation::Attack01,
	    EUEPokemonAttackAnimation::Attack02, EUEPokemonAttackAnimation::RangeAttack01,
	    EUEPokemonAttackAnimation::RangeAttack02};
	return Animation && Animation->PlayAttackAnimation(Attacks[AttackSlot - 1]);
}

bool UUEFieldPartnerSyncComponent::PlayLocalPartnerFieldAnimation(uint64 OwnerEntityId,
    EUEPokemonFieldAnimation Animation, int32 LoopCount)
{
	const FPartner *Partner = Partners.Find(OwnerEntityId);
	auto *Instance = Partner && Partner->Actor.IsValid() && !Partner->Actor->IsHidden()
	    ? Cast<UUEPokemonAnimInstance>(Partner->Actor->GetMesh()->GetAnimInstance()) : nullptr;
	return Instance && Instance->PlayFieldAnimation(Animation, LoopCount);
}

bool UUEFieldPartnerSyncComponent::ApplyPartnerServerState(uint64 OwnerEntityId,
                                                           const hhv::movement::State &State,
                                                           bool bTeleported, double ServerTimeSeconds)
{
	const FPartner *Partner = Partners.Find(OwnerEntityId);
	if (Partner == nullptr || !Partner->Actor.IsValid())
	{
		return false;
	}

	Partner->Actor->ApplyCoreSnapshot(State, ServerTimeSeconds, bTeleported);
	return true;
}

bool UUEFieldPartnerSyncComponent::RemovePartner(uint64 OwnerEntityId)
{
	FPartner Partner;
	if (!Partners.RemoveAndCopyValue(OwnerEntityId, Partner))
	{
		return false;
	}

	if (Partner.Actor.IsValid())
	{
		Partner.Actor->Destroy();
	}
	return true;
}

void UUEFieldPartnerSyncComponent::DestroyPartners()
{
	for (TPair<uint64, FPartner> &Pair : Partners)
	{
		if (Pair.Value.Actor.IsValid())
		{
			Pair.Value.Actor->Destroy();
		}
	}
	Partners.Empty();
	LocalNavigation.reset();
	LocalNavigationLoading = {};
	bLocalNavigationStarted = false;
	FollowAccumulator = FollowTime = 0.0;
	SetComponentTickEnabled(false);
}
