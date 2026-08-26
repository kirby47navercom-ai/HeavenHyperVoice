// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPokemonCharacter.h"

#include "../AI/UEAIController.h"
#include "../Animation/UEPokemonAnimInstance.h"
#include "UEPokemonSpeciesData.h"
#include "Server/UEPokemonServerComponent.h"
#include "Effects/UEPokemonSummonEffectComponent.h"
#include "UEPokemonSpeciesCatalog.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AUEPokemonCharacter::AUEPokemonCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AUEAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	ConfigureServerDrivenMovement();

	ServerComponent = CreateDefaultSubobject<UUEPokemonServerComponent>(TEXT("ServerComponent"));
	SummonEffectComponent = CreateDefaultSubobject<UUEPokemonSummonEffectComponent>(TEXT("SummonEffectComponent"));
}

void AUEPokemonCharacter::BeginPlay()
{
	Super::BeginPlay();

	ConfigureServerDrivenMovement();
	ApplyPokemonSpeciesData();
	TargetServerLocation = GetActorLocation();
	TargetServerRotation = GetActorRotation();
}

void AUEPokemonCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateServerDrivenMovement(DeltaSeconds);
}

void AUEPokemonCharacter::ApplyServerMoveSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot)
{
	ServerPokemonId = Snapshot.PokemonId;
	PokemonInstanceId = Snapshot.PokemonInstanceId;
	ServerSpeciesId = Snapshot.SpeciesId;
	SetRenderType(Snapshot.RenderType);
	ApplyServerStats(Snapshot.CurrentHP, Snapshot.MaxHP);
	ApplyServerAnimationSnapshot(Snapshot);
	ApplyServerMoveTarget(Snapshot.Location, Snapshot.Velocity, Snapshot.Rotation, Snapshot.bTeleported);
}

void AUEPokemonCharacter::SetPokemonSpeciesData(UUEPokemonSpeciesData* NewSpeciesData)
{
	if (PokemonSpeciesData == NewSpeciesData)
	{
		return;
	}

	PokemonSpeciesData = NewSpeciesData;
	ApplyPokemonSpeciesData();
}

void AUEPokemonCharacter::SetWildSpecies(int32 SpeciesNumber)
{
	// 서버 종족 번호는 카탈로그 조회와 디버그 식별에만 사용한다.
	ServerSpeciesId = FName(*FString::FromInt(SpeciesNumber));
	RenderType = EUEPokemonRenderType::Wild;

	// 카탈로그에 그 종족의 실제 모델이 있으면 그걸 쓴다. 아직 에셋이 없으면
	// 카탈로그가 비어 있고, 예전처럼 종족 색 큐브로 뜬다.
	if (PokemonSpeciesCatalog)
	{
		if (UUEPokemonSpeciesData* Data = PokemonSpeciesCatalog->Find(SpeciesNumber))
		{
			// SetPokemonSpeciesData 가 메시·캡슐·애니메이션까지 다 적용하고
			// ServerSpeciesId 를 데이터 쪽 이름으로 덮는다. 착색은 그 안에서
			// 알아서 빠진다 (SkeletalMesh 가 있으면 큐브를 안 건드린다).
			SetPokemonSpeciesData(Data);
			return;
		}
	}

	ApplyDebugAppearance();
}

FName AUEPokemonCharacter::GetPokemonSpeciesId() const
{
	return PokemonSpeciesData && !PokemonSpeciesData->SpeciesId.IsNone() ? PokemonSpeciesData->SpeciesId : ServerSpeciesId;
}

void AUEPokemonCharacter::SetRenderType(EUEPokemonRenderType NewRenderType)
{
	RenderType = NewRenderType;
}

void AUEPokemonCharacter::ConfigureServerDrivenMovement()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->GravityScale = 0.0f;
		MovementComponent->SetMovementMode(MOVE_None);
		MovementComponent->SetComponentTickEnabled(false);
	}
}

void AUEPokemonCharacter::ApplyPokemonSpeciesData()
{
	if (!PokemonSpeciesData)
	{
		return;
	}

	if (UCapsuleComponent* LocalCapsuleComponent = GetCapsuleComponent())
	{
		LocalCapsuleComponent->SetCapsuleSize(PokemonSpeciesData->CapsuleRadius, PokemonSpeciesData->CapsuleHalfHeight, true);
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (PokemonSpeciesData->SkeletalMesh)
		{
			MeshComponent->SetSkeletalMesh(PokemonSpeciesData->SkeletalMesh);
		}

		if (PokemonSpeciesData->AnimInstanceClass)
		{
			MeshComponent->SetAnimInstanceClass(PokemonSpeciesData->AnimInstanceClass);
		}

		MeshComponent->SetRelativeTransform(PokemonSpeciesData->MeshRelativeTransform);
	}

	// 이 값은 게임 스탯 스케일이라 uu/s 로 쓰려면 환산이 필요하다
	// (UEPokemonServerComponent 의 SpeciesMoveSpeedToUnrealUnits).
	ConfiguredMoveSpeed = PokemonSpeciesData->MoveSpeed;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = ConfiguredMoveSpeed;
		MovementComponent->MaxStepHeight = PokemonSpeciesData->MaxStepHeight;
		MovementComponent->SetWalkableFloorAngle(PokemonSpeciesData->WalkableFloorAngleDegrees);
		ConfigureServerDrivenMovement();
	}

	GetMesh()->SetRelativeLocation({0.0f,0.0f,-90.0f});

	MaxHP = FMath::Max(PokemonSpeciesData->MaxHP, 1.0f);
	CurrentHP = MaxHP;
	ServerSpeciesId = PokemonSpeciesData->SpeciesId;

	ApplyDebugAppearance();
}

void AUEPokemonCharacter::ApplyDebugAppearance()
{
	// 진짜 모델이 붙어 있으면 큐브를 건드리지 않는다.
	if (PokemonSpeciesData && PokemonSpeciesData->SkeletalMesh)
	{
		return;
	}

	// 한 번만 칠한다. 매번 CreateDynamicMaterialInstance 를 부르면 큐브가
	// 프레임마다 새 머티리얼로 바뀌며 깜빡인다.
	if (bDebugAppearanceApplied)
	{
		return;
	}
	bDebugAppearanceApplied = true;

	// 큐브는 BP_Pokemon 이 들고 있는 StaticMeshComponent 다. 런타임에 찾는다.
	UStaticMeshComponent* CubeComponent = FindComponentByClass<UStaticMeshComponent>();
	if (!CubeComponent)
	{
		return;
	}

	// 색은 종족 데이터 에셋이 소유한다. 값이 없으면 원본 머티리얼을 그대로 둔다.
	if (!PokemonSpeciesData || PokemonSpeciesData->DebugColor.A <= 0.0f)
	{
		return;
	}
	const FLinearColor Color = PokemonSpeciesData->DebugColor;

	// BasicShapeMaterial 에는 색 파라미터가 없어 이대로는 회색 큐브다. 색이
	// 실제로 나오게 하려면 Color(VectorParameter) 하나 있는 머티리얼을 큐브에
	// 물리면 된다 — 아래 관례 파라미터명들을 그대로 쓰면 코드 수정이 필요 없다.
	if (UMaterialInstanceDynamic* Dynamic = CubeComponent->CreateDynamicMaterialInstance(0))
	{
		Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
		Dynamic->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Dynamic->SetVectorParameterValue(TEXT("Tint"), Color);
	}
}

void AUEPokemonCharacter::ApplyServerStats(float ServerCurrentHP, float ServerMaxHP)
{
	if (ServerMaxHP <= 0.0f)
	{
		return;
	}

	MaxHP = FMath::Max(ServerMaxHP, 1.0f);
	CurrentHP = FMath::Clamp(ServerCurrentHP, 0.0f, MaxHP);
}

void AUEPokemonCharacter::ApplyServerMoveTarget(const FVector& ServerLocation, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bTeleported)
{
	ServerMoveStartLocation = GetActorLocation();
	ServerMoveStartRotation = GetActorRotation();
	TargetServerLocation = ServerLocation;
	TargetServerRotation = ServerRotation;
	ServerMoveElapsedSeconds = 0.0f;
	bHasServerMoveTarget = true;

	const float DistanceToServer = FVector::Dist(GetActorLocation(), ServerLocation);
	if (bTeleported || DistanceToServer >= ServerHardSnapDistance)
	{
		SetActorLocation(ServerLocation, false, nullptr, ETeleportType::TeleportPhysics);
		SetActorRotation(ServerRotation, ETeleportType::TeleportPhysics);

		// 순간이동 거리를 이동 속도로 취급하면 한 프레임 동안 달리기 애니메이션이 튄다.
		// 다음 일반 이동 스냅샷을 받을 때까지 정지 속도로 유지한다.
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
		bHasServerMoveTarget = false;
		return;
	}

	// 네트워크 패킷이 잠시 밀려 목표점이 멀어져도 한 프레임에 따라잡지 않는다.
	// 종별 DataAsset의 MoveSpeed를 상한으로 삼아 갑작스러운 가속과 떨림을 막는다.
	const float SnapshotVelocity = ServerVelocity.Size2D();
	const bool bHasAuthoritativeVelocity = SnapshotVelocity > UE_KINDA_SMALL_NUMBER;
	const float MaximumVisualSpeed = bHasAuthoritativeVelocity
		? SnapshotVelocity
		: FMath::Max(ConfiguredMoveSpeed, 1.0f);
	const float RequiredMoveSeconds = DistanceToServer / MaximumVisualSpeed;

	// 로컬 서버 컴포넌트는 산책 속도 배율이 적용된 실제 속도를 함께 보낸다.
	// 이 값을 무시하고 종별 최고속도로 움직이면 빠르게 이동한 뒤 멈추는 동작이
	// 20Hz마다 반복되므로, 목표 거리와 실제 속도로 정확한 구간 시간을 계산한다.
	ServerMoveDurationSeconds = bHasAuthoritativeVelocity
		? FMath::Max(RequiredMoveSeconds, UE_SMALL_NUMBER)
		: FMath::Max(ServerSnapshotIntervalSeconds, RequiredMoveSeconds);
}

void AUEPokemonCharacter::ApplyServerAnimationSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot)
{
	ServerAnimationState = Snapshot.AnimationState;

	if (Snapshot.AnimationEvent == EUEPokemonAnimationEvent::None)
	{
		return;
	}

	LastServerAnimationEvent = Snapshot.AnimationEvent;
	LastServerAnimationEventTimeSeconds = Snapshot.ServerTimeSeconds;
	LastServerAnimationEventDurationSeconds = Snapshot.EventDurationSeconds;

	// 서버가 선택한 필드 행동은 현재 종의 AnimInstance가 실제 시퀀스로 변환해 재생한다.
	// DataAsset에 없는 시퀀스는 AnimInstance에서 자동으로 건너뛴다.
	if (Snapshot.AnimationEvent == EUEPokemonAnimationEvent::FieldAnimationStarted)
	{
		if (UUEPokemonAnimInstance* PokemonAnimInstance = Cast<UUEPokemonAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			PokemonAnimInstance->PlayFieldAnimation(Snapshot.FieldAnimation, Snapshot.FieldAnimationLoopCount);
		}
	}
	else if (Snapshot.AnimationEvent == EUEPokemonAnimationEvent::AttackStarted)
	{
		// 서버가 승인한 공격 종류만 재생해 클라이언트 입력과 실제 포켓몬 상태가 어긋나지 않게 한다.
		if (UUEPokemonAnimInstance* PokemonAnimInstance = Cast<UUEPokemonAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			PokemonAnimInstance->PlayAttackAnimation(Snapshot.AttackAnimation, Snapshot.AttackAnimationLoopCount);
		}
	}

	BP_OnServerAnimationEvent(Snapshot.AnimationEvent, Snapshot);
}

void AUEPokemonCharacter::UpdateServerDrivenMovement(float DeltaSeconds)
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!bHasServerMoveTarget)
	{
		// 서버 스냅샷 사이에 이미 목표점에 도착했다면 남아 있는 목표 속도를 지운다.
		// 이 값이 남으면 몸은 멈췄는데 걷기 애니메이션만 계속 재생된다.
		if (MovementComponent)
		{
			MovementComponent->Velocity = FVector::ZeroVector;
		}
		return;
	}

	const FVector PreviousLocation = GetActorLocation();
	ServerMoveElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
	const float SafeMoveDuration = FMath::Max(ServerMoveDurationSeconds, UE_SMALL_NUMBER);
	const float MoveAlpha = FMath::Clamp(ServerMoveElapsedSeconds / SafeMoveDuration, 0.0f, 1.0f);

	// VInterpTo는 새 20Hz 좌표가 올 때마다 빠르게 출발했다가 느려져 속도가 맥동한다.
	// 선형 보간은 구간 전체의 속도를 일정하게 유지해 몸과 발의 부들거림을 없앤다.
	const FVector NewLocation = FMath::Lerp(ServerMoveStartLocation, TargetServerLocation, MoveAlpha);
	const FQuat NewRotation = FQuat::Slerp(
		ServerMoveStartRotation.Quaternion(),
		TargetServerRotation.Quaternion(),
		MoveAlpha).GetNormalized();

	SetActorLocation(NewLocation, false);
	SetActorRotation(NewRotation);

	if (MovementComponent)
	{
		// 애니메이션에는 서버가 지시한 목표 속도가 아니라 화면에서 실제로 이동한 속도를 전달한다.
		// 보간 중 남은 이동과 정지 구간까지 같은 기준을 사용해야 발이 땅에서 덜 미끄러진다.
		const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, UE_SMALL_NUMBER);
		MovementComponent->Velocity = (GetActorLocation() - PreviousLocation) / SafeDeltaSeconds;
	}

	if (MoveAlpha >= 1.0f)
	{
		bHasServerMoveTarget = false;
	}
}
