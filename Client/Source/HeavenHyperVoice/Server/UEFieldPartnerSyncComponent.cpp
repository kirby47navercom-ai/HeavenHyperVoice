#include "UEFieldPartnerSyncComponent.h"

#include "../Pokemon/UEPokemonCharacter.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	// 주인 앞 오른쪽에서 시작한다. 첫 프레임에 가까운 쪽으로 다시 고른다.
	const FVector SpawnLocation = StandingSpot(*OwnerActor, 1.0f);
	const FTransform SpawnTransform(OwnerActor->GetActorRotation(), SpawnLocation);

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

	// 주인과 부딪히면 서로 밀어내다 둘 다 튄다. 관통은 AvoidOwner 가 막는다.
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

FVector UUEFieldPartnerSyncComponent::StandingSpot(const AActor& Owner, float SideSign) const
{
	// 예전 FollowOwnerAction::CalculateOffsetTarget 과 같은 자리다.
	return Owner.GetActorLocation()
		+ Owner.GetActorForwardVector() * FollowForwardOffset
		+ Owner.GetActorRightVector() * FollowSideOffset * SideSign;
}

FVector UUEFieldPartnerSyncComponent::AvoidOwner(const FVector& From, const FVector& To,
	const AActor& Owner, float SideSign) const
{
	const FVector ToTarget = To - From;
	const float Length = ToTarget.Size2D();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return To;
	}

	// 주인이 선분 위에 얹혀 있는지 본다. 뒤쪽이나 너머에 있으면 지나갈 일이 없다.
	const FVector Direction = FVector(ToTarget.X, ToTarget.Y, 0.0f) / Length;
	const FVector ToOwner = Owner.GetActorLocation() - From;
	const float Along = FVector::DotProduct(FVector(ToOwner.X, ToOwner.Y, 0.0f), Direction);
	if (Along <= 0.0f || Along >= Length)
	{
		return To;
	}

	const FVector Closest = FVector(ToOwner.X, ToOwner.Y, 0.0f) - Direction * Along;
	if (Closest.Size2D() >= OwnerAvoidRadius)
	{
		return To;
	}

	// 관통 경로다. 주인 옆(가려는 쪽)을 먼저 들렀다 간다. 거기서 목표점까지는
	// 같은 쪽이라 다시 주인을 가로지르지 않는다.
	return Owner.GetActorLocation()
		+ Owner.GetActorRightVector() * (OwnerAvoidRadius + FollowSideOffset * 0.5f) * SideSign;
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
		const FVector Here = Actor->GetActorLocation();

		// 주인 앞 좌우 중 가까운 쪽. 그냥 매번 가까운 쪽을 고르면 주인이 방향을
		// 틀 때마다 좌우가 번갈아 뒤집히므로, 반대쪽이 확실히 가까울 때만 옮긴다.
		const FVector Kept = StandingSpot(*Owner, Partner.SideSign);
		const FVector Other = StandingSpot(*Owner, -Partner.SideSign);
		if (FVector::Dist2D(Here, Other) + SideSwitchMargin < FVector::Dist2D(Here, Kept))
		{
			Partner.SideSign = -Partner.SideSign;
		}

		const FVector Spot = StandingSpot(*Owner, Partner.SideSign);
		const float SpotDistance = FVector::Dist2D(Here, Spot);

		if (SpotDistance <= ArriveDistance)
		{
			// 도착. 목표를 새로 주지 않으면 UpdateServerDrivenMovement 가 속도를
			// 0 으로 떨어뜨려 서 있는 애니메이션으로 돌아간다.
			Partner.IdleSeconds += FMath::Max(DeltaTime, 0.0f);

			// 잠시 서 있으면 주인 쪽으로 몸을 돌린다. 바로 돌리면 주인이 조금만
			// 움직여도 파트너가 계속 홱홱 돈다.
			if (Partner.IdleSeconds >= FaceOwnerDelay)
			{
				const FVector ToOwner = Owner->GetActorLocation() - Here;
				if (ToOwner.Size2D() > KINDA_SMALL_NUMBER)
				{
					const FRotator Facing(0.0f, ToOwner.Rotation().Yaw, 0.0f);
					Actor->SetActorRotation(
						FMath::RInterpTo(Actor->GetActorRotation(), Facing, DeltaTime, 6.0f));
				}
			}
			continue;
		}

		Partner.IdleSeconds = 0.0f;

		// 주인을 뚫고 가지 않는다. 가로지르는 경로면 옆으로 먼저 돈다.
		const FVector Target = AvoidOwner(Here, Spot, *Owner, Partner.SideSign);
		const FVector Offset = Target - Here;
		const float Distance = Offset.Size2D();
		if (Distance <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// 종족 이동속도를 그대로 쓰면 그보다 빠른 주인을 영원히 못 따라잡고,
		// 간격이 벌어지다 순간이동 판정에 걸린다. 주인 속도를 바닥으로 삼고
		// 뒤처진 만큼 더 붙인다.
		const float OwnerSpeed = Owner->GetVelocity().Size2D();
		const float BaseSpeed = FMath::Max(OwnerSpeed, Actor->GetConfiguredMoveSpeed());
		const float ChaseSpeed = FMath::Min(
			BaseSpeed + FMath::Max(Distance - ArriveDistance, 0.0f) * CatchUpGain,
			MaxFollowSpeed);

		// 속도를 명시하면 ApplyServerMoveTarget 이 종족 이동속도 상한 대신 이
		// 값으로 구간 시간을 잡는다.
		Actor->ApplyServerMoveTarget(
			Target,
			Offset.GetSafeNormal2D() * ChaseSpeed,
			FRotator(0.0f, Offset.Rotation().Yaw, 0.0f),
			/*bTeleported=*/false);
	}
}
