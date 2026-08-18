// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPokemonCharacter.h"

#include "../AI/UEAIController.h"
#include "../Data/UEPokemonAnimationDataAsset.h"
#include "../UEGameplayTags.h"
#include "Server/UEPokemonServerComponent.h"
#include "UEPokemonSpeciesData.h"
#include "UEPokemonTestServerComponent.h"

#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"

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
	// 기본 배치 포켓몬은 기존 테스트 컴포넌트만 사용하고, 월드 스폰 데이터가 올 때만 서버 시뮬레이션을 켠다.
	ServerComponent->SetServerSimulationEnabled(false);
	TestServerComponent = CreateDefaultSubobject<UUEPokemonTestServerComponent>(TEXT("TestServerComponent"));

	static ConstructorHelpers::FObjectFinder<UUEPokemonAnimationDataAsset> PokemonAnimationDataFinder(
		TEXT("/Game/Data/Animation/DA_PokemonAnimation"));
	if (PokemonAnimationDataFinder.Succeeded())
	{
		PokemonAnimationData = PokemonAnimationDataFinder.Object;
	}
}

void AUEPokemonCharacter::BeginPlay()
{
	Super::BeginPlay();
	ConfigureServerDrivenMovement();
	ApplyPokemonSpeciesData();
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	TargetServerLocation = GetActorLocation();
	TargetServerRotation = GetActorRotation();
}

void AUEPokemonCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateServerDrivenMovement(DeltaSeconds);
	UpdatePokemonAnimation();
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

FName AUEPokemonCharacter::GetPokemonSpeciesId() const
{
	return PokemonSpeciesData && !PokemonSpeciesData->SpeciesId.IsNone()
		? PokemonSpeciesData->SpeciesId
		: ServerSpeciesId;
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

	ConfiguredMoveSpeed = PokemonSpeciesData->MoveSpeed;
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = ConfiguredMoveSpeed;
		MovementComponent->MaxStepHeight = PokemonSpeciesData->MaxStepHeight;
		MovementComponent->SetWalkableFloorAngle(PokemonSpeciesData->WalkableFloorAngleDegrees);
		ConfigureServerDrivenMovement();
	}

	MaxHP = FMath::Max(PokemonSpeciesData->MaxHP, 1.0f);
	CurrentHP = MaxHP;
	ServerSpeciesId = PokemonSpeciesData->SpeciesId;
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
	BP_OnServerAnimationEvent(Snapshot.AnimationEvent, Snapshot);
}

void AUEPokemonCharacter::ApplyServerMoveTarget(const FVector& ServerLocation, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bTeleported)
{
	TargetServerLocation = ServerLocation;
	TargetServerVelocity = ServerVelocity;
	TargetServerRotation = ServerRotation;
	bHasServerMoveTarget = true;

	const float DistanceToServer = FVector::Dist(GetActorLocation(), ServerLocation);
	if (bTeleported || DistanceToServer >= ServerHardSnapDistance)
	{
		SetActorLocation(ServerLocation, false, nullptr, ETeleportType::TeleportPhysics);
		SetActorRotation(ServerRotation, ETeleportType::TeleportPhysics);
		GetCharacterMovement()->Velocity = ServerVelocity;
		bHasServerMoveTarget = false;
	}
}

void AUEPokemonCharacter::UpdateServerDrivenMovement(float DeltaSeconds)
{
	if (!bHasServerMoveTarget)
	{
		return;
	}

	const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), TargetServerLocation, DeltaSeconds, ServerLocationInterpSpeed);
	const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetServerRotation, DeltaSeconds, ServerRotationInterpSpeed);

	SetActorLocation(NewLocation, false);
	SetActorRotation(NewRotation);
	GetCharacterMovement()->Velocity = TargetServerVelocity;

	if (FVector::DistSquared(NewLocation, TargetServerLocation) <= 1.0f)
	{
		SetActorLocation(TargetServerLocation, false);
		bHasServerMoveTarget = false;
	}
}

void AUEPokemonCharacter::UpdatePokemonAnimation()
{
	if (!PokemonAnimationData || !GetMesh() || !GetMesh()->GetSkeletalMeshAsset())
	{
		return;
	}

	const float GroundSpeed = GetVelocity().Size2D();
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const bool bFalling = MovementComponent && MovementComponent->IsFalling();

	const UAnimSequence* DesiredSequence = nullptr;
	bool bLoop = true;
	if (bFalling)
	{
		DesiredSequence = PokemonAnimationData->FindSequenceByTag(UEGameplayTags::State_Character_Fall);
	}
	else if (GroundSpeed >= 300.0f)
	{
		DesiredSequence = PokemonAnimationData->FindSequenceByTag(UEGameplayTags::State_Character_Run);
	}
	else if (GroundSpeed > 3.0f)
	{
		DesiredSequence = PokemonAnimationData->FindSequenceByTag(UEGameplayTags::State_Character_Walk);
	}
	else
	{
		DesiredSequence = PokemonAnimationData->FindSequenceByTag(UEGameplayTags::State_Character_Idle);
	}

	PlayPokemonAnimation(const_cast<UAnimSequence*>(DesiredSequence), bLoop);
}

void AUEPokemonCharacter::PlayPokemonAnimation(UAnimSequence* Sequence, bool bLoop)
{
	if (!Sequence || CurrentPokemonAnimation == Sequence)
	{
		return;
	}

	CurrentPokemonAnimation = Sequence;

	// 포켓몬은 서버 위치 보정을 받더라도 화면에서는 현재 속도에 맞는 루프 애니메이션을 재생한다.
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	GetMesh()->PlayAnimation(Sequence, bLoop);
}
