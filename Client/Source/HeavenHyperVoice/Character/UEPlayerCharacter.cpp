// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPlayerCharacter.h"

#include "../Component/UEPlayerMovementSyncComponent.h"
#include "../Pokemon/UEPokemonCharacter.h"
#include "../Pokemon/UEPokemonTestServerComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

AUEPlayerCharacter::AUEPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 420.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	MovementSyncComponent = CreateDefaultSubobject<UUEPlayerMovementSyncComponent>(TEXT("MovementSyncComponent"));

	static ConstructorHelpers::FClassFinder<AUEPokemonCharacter> DefaultPokemonClass(TEXT("/Game/Pokemon/BP_Pokemon"));
	if (DefaultPokemonClass.Succeeded())
	{
		PokemonCompanionClass = DefaultPokemonClass.Class;
	}
}

void AUEPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AUEPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PokemonDespawnTimerHandle);
	}

	if (IsValid(SpawnedPokemon))
	{
		SpawnedPokemon->Destroy();
		SpawnedPokemon = nullptr;
	}

	PendingDespawnPokemon = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AUEPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyLocalMovementInput();
}

FVector AUEPlayerCharacter::GetDesiredMovementDirection() const
{
	return GetMoveDirectionFromInput(MovementInput, GetControlRotation());
}

bool AUEPlayerCharacter::IsPokemonCompanionSpawned() const
{
	return IsValid(SpawnedPokemon) && !bPokemonDespawnInProgress;
}

void AUEPlayerCharacter::SetMovementInput(const FVector2D& NewMovementInput)
{
	MovementInput = NewMovementInput.GetClampedToMaxSize(1.0f);
}

void AUEPlayerCharacter::TogglePokemonCompanion()
{
	if (bPokemonDespawnInProgress)
	{
		return;
	}

	if (IsValid(SpawnedPokemon))
	{
		RequestDespawnPokemonCompanion();
		return;
	}

	TrySpawnPokemonCompanion();
}

void AUEPlayerCharacter::ApplyLocalMovementInput()
{
	const FVector DesiredDirection = GetMoveDirectionFromInput(MovementInput, GetControlRotation());
	if (DesiredDirection.IsNearlyZero())
	{
		return;
	}

	// Movement follows camera yaw; idle camera rotation does not rotate the character.
	const float InputStrength = FMath::Clamp(MovementInput.Size(), 0.0f, 1.0f);
	AddMovementInput(DesiredDirection, InputStrength);
}

FVector AUEPlayerCharacter::GetCameraForwardAxis(const FRotator& ViewRotation) const
{
	const FRotator CameraYawRotation(0.0f, ViewRotation.Yaw, 0.0f);
	return FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::X).GetSafeNormal2D();
}

FVector AUEPlayerCharacter::GetCameraRightAxis(const FRotator& ViewRotation) const
{
	const FRotator CameraYawRotation(0.0f, ViewRotation.Yaw, 0.0f);
	return FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::Y).GetSafeNormal2D();
}

FVector AUEPlayerCharacter::GetMoveDirectionFromInput(const FVector2D& Input, const FRotator& ViewRotation) const
{
	const FVector Direction = GetCameraForwardAxis(ViewRotation) * Input.X + GetCameraRightAxis(ViewRotation) * Input.Y;
	return Direction.GetSafeNormal();
}

void AUEPlayerCharacter::ApplyServerMovementCorrection(const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bUseHardCorrection)
{
	const ETeleportType CorrectionTeleportType = bUseHardCorrection ? ETeleportType::TeleportPhysics : ETeleportType::None;

	// Server correction updates the physical character state, then local movement continues next tick.
	SetActorLocation(ServerPosition, false, nullptr, CorrectionTeleportType);
	SetActorRotation(ServerRotation, CorrectionTeleportType);
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->Velocity = ServerVelocity;
	}
}

bool AUEPlayerCharacter::TrySpawnPokemonCompanion()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TSubclassOf<AUEPokemonCharacter> ClassToSpawn = PokemonCompanionClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AUEPokemonCharacter::StaticClass();
	}

	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::Spawning);
	const HHV::PokemonAI::CompanionContext Context = MakePokemonLifecycleContext(HHV::PokemonAI::RequestedAction::Spawn);
	const HHV::PokemonAI::Command SpawnCommand = PokemonLifecycleBrain.Tick(Context);
	if (SpawnCommand.Type != HHV::PokemonAI::CommandType::Spawn)
	{
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
		return false;
	}

	FVector SpawnLocation;
	FRotator SpawnRotation;
	if (!ResolvePokemonSpawnTransform(SpawnCommand, SpawnLocation, SpawnRotation))
	{
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
		return false;
	}

	BP_OnPokemonSpawnRequested(SpawnLocation, SpawnRotation);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AUEPokemonCharacter* NewPokemon = World->SpawnActor<AUEPokemonCharacter>(ClassToSpawn, SpawnLocation, SpawnRotation, SpawnParameters);
	if (!NewPokemon)
	{
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
		return false;
	}

	SpawnedPokemon = NewPokemon;
	if (UUEPokemonTestServerComponent* TestServerComponent = NewPokemon->GetTestServerComponent())
	{
		TestServerComponent->SetFollowTargetActor(this);
	}

	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
	BP_OnPokemonSpawned(NewPokemon);
	return true;
}

void AUEPlayerCharacter::RequestDespawnPokemonCompanion()
{
	if (!IsValid(SpawnedPokemon))
	{
		SpawnedPokemon = nullptr;
		return;
	}

	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::Despawning);
	const HHV::PokemonAI::CompanionContext Context = MakePokemonLifecycleContext(HHV::PokemonAI::RequestedAction::Despawn);
	const HHV::PokemonAI::Command DespawnCommand = PokemonLifecycleBrain.Tick(Context);
	if (DespawnCommand.Type != HHV::PokemonAI::CommandType::Despawn)
	{
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
		return;
	}

	PendingDespawnPokemon = SpawnedPokemon;
	bPokemonDespawnInProgress = true;
	BP_OnPokemonDespawnRequested(PendingDespawnPokemon.Get());

	if (PokemonDespawnDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PokemonDespawnTimerHandle, this, &ThisClass::FinishPokemonDespawn, PokemonDespawnDelay, false);
		return;
	}

	FinishPokemonDespawn();
}

void AUEPlayerCharacter::FinishPokemonDespawn()
{
	AUEPokemonCharacter* PokemonToDestroy = PendingDespawnPokemon.Get();
	if (!IsValid(PokemonToDestroy) && IsValid(SpawnedPokemon))
	{
		PokemonToDestroy = SpawnedPokemon.Get();
	}

	if (IsValid(PokemonToDestroy))
	{
		PokemonToDestroy->Destroy();
	}

	if (SpawnedPokemon.Get() == PokemonToDestroy || !IsValid(SpawnedPokemon))
	{
		SpawnedPokemon = nullptr;
	}

	PendingDespawnPokemon = nullptr;
	bPokemonDespawnInProgress = false;
	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
	BP_OnPokemonDespawned();
}

HHV::PokemonAI::CompanionContext AUEPlayerCharacter::MakePokemonLifecycleContext(HHV::PokemonAI::RequestedAction ActionRequest) const
{
	HHV::PokemonAI::CompanionContext Context;
	Context.OwnerLocation = ToServerVec3(GetActorLocation());
	Context.OwnerYawDegrees = GetActorRotation().Yaw;
	Context.ActionRequest = ActionRequest;
	Context.Agent = MakePokemonAgentSettings();
	Context.PokemonLocation = IsValid(SpawnedPokemon)
		? ToServerVec3(SpawnedPokemon->GetActorLocation())
		: Context.OwnerLocation;
	return Context;
}

HHV::Map::AgentSettings AUEPlayerCharacter::MakePokemonAgentSettings() const
{
	HHV::Map::AgentSettings Agent;
	TSubclassOf<AUEPokemonCharacter> ClassToInspect = PokemonCompanionClass;
	if (!ClassToInspect)
	{
		ClassToInspect = AUEPokemonCharacter::StaticClass();
	}

	const AUEPokemonCharacter* DefaultPokemon = ClassToInspect ? ClassToInspect->GetDefaultObject<AUEPokemonCharacter>() : nullptr;
	if (!DefaultPokemon)
	{
		return Agent;
	}

	if (const UCapsuleComponent* DefaultCapsuleComponent = DefaultPokemon->GetCapsuleComponent())
	{
		Agent.CapsuleRadius = DefaultCapsuleComponent->GetScaledCapsuleRadius();
		Agent.CapsuleHalfHeight = DefaultCapsuleComponent->GetScaledCapsuleHalfHeight();
	}

	if (const UCharacterMovementComponent* MovementComponent = DefaultPokemon->GetCharacterMovement())
	{
		Agent.MaxStepHeight = MovementComponent->MaxStepHeight;
		Agent.WalkableFloorAngleDegrees = MovementComponent->GetWalkableFloorAngle();
	}

	return Agent;
}

bool AUEPlayerCharacter::ResolvePokemonSpawnTransform(const HHV::PokemonAI::Command& SpawnCommand, FVector& OutLocation, FRotator& OutRotation) const
{
	OutRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);

	for (const HHV::Map::Vec3& Candidate : SpawnCommand.PathPoints)
	{
		if (TryResolvePokemonSpawnCandidate(ToUnrealVector(Candidate), OutRotation, OutLocation))
		{
			return true;
		}
	}

	return TryResolvePokemonSpawnCandidate(ToUnrealVector(SpawnCommand.TargetLocation), OutRotation, OutLocation);
}

bool AUEPlayerCharacter::TryResolvePokemonSpawnCandidate(const FVector& CandidateLocation, const FRotator& SpawnRotation, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const HHV::Map::AgentSettings Agent = MakePokemonAgentSettings();
	FVector AdjustedLocation = CandidateLocation;

	FHitResult GroundHit;
	FCollisionQueryParams GroundTraceParams(TEXT("PokemonSpawnGroundTrace"), false, this);
	const FVector TraceStart = CandidateLocation + FVector(0.0f, 0.0f, PokemonSpawnGroundTraceDistance);
	const FVector TraceEnd = CandidateLocation - FVector(0.0f, 0.0f, PokemonSpawnGroundTraceDistance);
	if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundTraceParams))
	{
		AdjustedLocation.Z = GroundHit.ImpactPoint.Z + Agent.CapsuleHalfHeight;
	}

	const FCollisionShape SpawnShape = FCollisionShape::MakeCapsule(Agent.CapsuleRadius, Agent.CapsuleHalfHeight);
	const FCollisionQueryParams SpawnOverlapParams(TEXT("PokemonSpawnOverlap"), false);
	const bool bBlocked = World->OverlapBlockingTestByChannel(
		AdjustedLocation,
		SpawnRotation.Quaternion(),
		PokemonSpawnCollisionChannel,
		SpawnShape,
		SpawnOverlapParams
	);

	if (bBlocked)
	{
		return false;
	}

	OutLocation = AdjustedLocation;
	return true;
}

HHV::Map::Vec3 AUEPlayerCharacter::ToServerVec3(const FVector& Vector)
{
	return HHV::Map::Vec3{
		static_cast<float>(Vector.X),
		static_cast<float>(Vector.Y),
		static_cast<float>(Vector.Z)
	};
}

FVector AUEPlayerCharacter::ToUnrealVector(const HHV::Map::Vec3& Vector)
{
	return FVector(Vector.X, Vector.Y, Vector.Z);
}
