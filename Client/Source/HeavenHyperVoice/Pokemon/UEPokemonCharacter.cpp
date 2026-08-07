// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPokemonCharacter.h"

#include "../AI/UEAIController.h"
#include "UEPokemonTestServerComponent.h"

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

	TestServerComponent = CreateDefaultSubobject<UUEPokemonTestServerComponent>(TEXT("TestServerComponent"));
}

void AUEPokemonCharacter::BeginPlay()
{
	Super::BeginPlay();

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
	ApplyServerMoveTarget(Snapshot.Location, Snapshot.Velocity, Snapshot.Rotation, Snapshot.bTeleported);
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
