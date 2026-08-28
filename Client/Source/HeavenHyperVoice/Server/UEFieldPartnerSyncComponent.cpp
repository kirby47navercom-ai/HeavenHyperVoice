#include "UEFieldPartnerSyncComponent.h"

#include "../Pokemon/UEPokemonCharacter.h"

#include "Engine/World.h"

UUEFieldPartnerSyncComponent::UUEFieldPartnerSyncComponent()
{
	// 파트너는 서버 스냅샷이 아니라 주인의 현재 위치를 쫓는다. 20Hz 로 끊어
	// 따라가면 주인보다 눈에 띄게 늦으므로 매 프레임 목표를 다시 잡는다.
	PrimaryComponentTick.bCanEverTick = true;
}

void UUEFieldPartnerSyncComponent::SetPartnerPokemonClass(TSubclassOf<AUEPokemonCharacter> InPartnerPokemonClass)
{
	if (InPartnerPokemonClass)
	{
		PartnerPokemonClass = InPartnerPokemonClass;
	}
}

void UUEFieldPartnerSyncComponent::AddPartner(uint64 OwnerEntityId, AActor* OwnerActor, int32 DexNumber)
{
	if (DexNumber <= 0 || OwnerActor == nullptr || Partners.Contains(OwnerEntityId))
	{
		return;
	}

	UWorld* World = GetWorld();
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

	const FTransform SpawnTransform(OwnerActor->GetActorRotation(), OwnerActor->GetActorLocation());
	AUEPokemonCharacter* PartnerActor = World->SpawnActorDeferred<AUEPokemonCharacter>(
		PartnerPokemonClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!PartnerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("FieldPartnerSync: partner spawn failed for owner %llu"), OwnerEntityId);
		return;
	}

	PartnerActor->AutoPossessAI = EAutoPossessAI::Disabled;
	PartnerActor->AIControllerClass = nullptr;
	PartnerActor->FinishSpawning(SpawnTransform);

	// 주인과 부딪히면 서로 밀어내다 둘 다 튄다. 파트너는 보여 주기만 한다.
	PartnerActor->SetActorEnableCollision(false);

	// 종족은 도감번호로 찾는다. 엔티티 id 는 서버 것이 아니라 주인 것을 그대로
	// 쓴다 — 서버에 파트너 엔티티가 없어서 겹칠 번호도 없다.
	PartnerActor->InitializeServerEntity(
		static_cast<int64>(OwnerEntityId), DexNumber, EUEPokemonRenderType::Own);

	// 기본값(300)은 서버 스냅샷을 받는 야생 포켓몬 기준이다. 주인을 쫓는 파트너는
	// 주인이 달리기만 해도 그만큼 뒤처져서, 그대로 두면 매 프레임 순간이동한다.
	PartnerActor->SetServerHardSnapDistance(TeleportDistance);

	Partners.Add(OwnerEntityId, FPartner{OwnerActor, PartnerActor});
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
	for (TPair<uint64, FPartner>& Pair : Partners)
	{
		if (Pair.Value.Actor.IsValid())
		{
			Pair.Value.Actor->Destroy();
		}
	}
	Partners.Empty();
}

void UUEFieldPartnerSyncComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (auto It = Partners.CreateIterator(); It; ++It)
	{
		FPartner& Partner = It.Value();

		// 주인이 사라졌는데 파트너만 남으면 아무도 안 따라가는 포켓몬이 서 있는다.
		if (!Partner.Owner.IsValid())
		{
			if (Partner.Actor.IsValid())
			{
				Partner.Actor->Destroy();
			}
			It.RemoveCurrent();
			continue;
		}
		if (!Partner.Actor.IsValid())
		{
			It.RemoveCurrent();
			continue;
		}

		const AActor* Owner = Partner.Owner.Get();
		AUEPokemonCharacter* Actor = Partner.Actor.Get();

		const FVector FollowPoint = Owner->GetActorLocation()
			- Owner->GetActorForwardVector() * FollowDistance
			+ Owner->GetActorRightVector() * FollowSideOffset;

		const FVector Offset = FollowPoint - Actor->GetActorLocation();
		const float Distance = Offset.Size2D();
		if (Distance <= FollowTolerance)
		{
			// 목표점 안에 있으면 목표를 새로 주지 않는다. UpdateServerDrivenMovement
			// 가 속도를 0 으로 떨어뜨려 서 있는 애니메이션으로 돌아간다.
			continue;
		}

		// 종족 이동속도를 그대로 쓰면 그보다 빠른 주인을 영원히 못 따라잡고,
		// 간격이 벌어지다 순간이동 판정에 걸린다. 주인 속도를 바닥으로 삼고
		// 뒤처진 만큼 더 붙인다.
		const float OwnerSpeed = Owner->GetVelocity().Size2D();
		const float BaseSpeed = FMath::Max(OwnerSpeed, Actor->GetConfiguredMoveSpeed());
		const float ChaseSpeed = FMath::Min(
			BaseSpeed + (Distance - FollowTolerance) * CatchUpGain,
			MaxFollowSpeed);

		// 속도를 명시하면 ApplyServerMoveTarget 이 종족 이동속도 상한 대신 이
		// 값으로 구간 시간을 잡는다.
		Actor->ApplyServerMoveTarget(
			FollowPoint,
			Offset.GetSafeNormal2D() * ChaseSpeed,
			FRotator(0.0f, Offset.Rotation().Yaw, 0.0f),
			/*bTeleported=*/false);
	}
}
