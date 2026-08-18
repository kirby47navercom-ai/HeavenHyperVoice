// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPokemonCharacter.h"

#include "../AI/UEAIController.h"
#include "UEPokemonSpeciesData.h"
#include "Server/UEPokemonServerComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"

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
