// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPokemonTestServerComponent.h"

#include "UEPokemonCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

UUEPokemonTestServerComponent::UUEPokemonTestServerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UUEPokemonTestServerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AUEPokemonCharacter* PokemonCharacter = GetPokemonOwner())
	{
		ServerSimulatedLocation = PokemonCharacter->GetActorLocation();
		ServerSimulatedRotation = PokemonCharacter->GetActorRotation();
	}

	TryLoadServerMap();
}

void UUEPokemonTestServerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnableTestServer)
	{
		return;
	}

	const float ServerTickInterval = 1.0f / FMath::Max(ServerTickRate, 1.0f);
	ServerTickAccumulator += DeltaTime;
	if (ServerTickAccumulator < ServerTickInterval)
	{
		return;
	}

	const float SimulatedDeltaSeconds = ServerTickAccumulator;
	ServerTickAccumulator = 0.0f;
	RunServerSimulationTick(SimulatedDeltaSeconds);
}

void UUEPokemonTestServerComponent::SetFollowTargetActor(AActor* NewFollowTargetActor)
{
	FollowTargetActor = NewFollowTargetActor;
}

void UUEPokemonTestServerComponent::TryLoadServerMap()
{
	const FString MapFilePath = ResolveServerMapFilePath();
	if (MapFilePath.IsEmpty() || !FPaths::FileExists(MapFilePath))
	{
		bServerMapLoaded = false;
		return;
	}

	bServerMapLoaded = ServerMapRuntime.LoadFromFile(TCHAR_TO_UTF8(*MapFilePath));
	UE_LOG(
		LogTemp,
		Display,
		TEXT("PokemonTestServer: server map %s: %s"),
		bServerMapLoaded ? TEXT("loaded") : TEXT("failed"),
		*MapFilePath
	);
}

void UUEPokemonTestServerComponent::RunServerSimulationTick(float DeltaSeconds)
{
	AUEPokemonCharacter* PokemonCharacter = GetPokemonOwner();
	const AActor* CurrentFollowTargetActor = ResolveFollowTargetActor();
	if (!PokemonCharacter || !CurrentFollowTargetActor)
	{
		return;
	}

	const HHV::PokemonAI::CompanionContext Context = MakeCompanionContext(*PokemonCharacter, *CurrentFollowTargetActor, DeltaSeconds);
	const HHV::PokemonAI::Command Command = TestServerBrain.Tick(Context);
	ApplyServerCommand(*PokemonCharacter, Command, DeltaSeconds);
}

void UUEPokemonTestServerComponent::ApplyServerCommand(AUEPokemonCharacter& PokemonCharacter, const HHV::PokemonAI::Command& Command, float DeltaSeconds)
{
	using HHV::PokemonAI::CommandType;

	switch (Command.Type)
	{
	case CommandType::MoveTo:
	{
		const FVector TargetLocation = ToUnrealVector(Command.TargetLocation);
		const FVector ToTarget = TargetLocation - ServerSimulatedLocation;
		const float DistanceToTarget = ToTarget.Size();
		const float AcceptanceRadius = FMath::Max(Command.AcceptanceRadius, 1.0f);
		if (DistanceToTarget <= AcceptanceRadius)
		{
			ServerSimulatedLocation = TargetLocation;
			ServerSimulatedVelocity = FVector::ZeroVector;
		}
		else
		{
			const FVector MoveDirection = ToTarget / DistanceToTarget;
			const float MoveDistance = FMath::Min(ServerMoveSpeed * DeltaSeconds, DistanceToTarget);
			ServerSimulatedLocation += MoveDirection * MoveDistance;
			ServerSimulatedVelocity = MoveDirection * ServerMoveSpeed;
			ServerSimulatedRotation = MoveDirection.ToOrientationRotator();
		}

		// The local test server sends the same movement snapshot the real server will send later.
		SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, false);
		break;
	}
	case CommandType::Teleport:
		ServerSimulatedLocation = ToUnrealVector(Command.TargetLocation);
		ServerSimulatedVelocity = FVector::ZeroVector;
		SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, true);
		break;
	case CommandType::Stop:
	case CommandType::None:
	default:
		ServerSimulatedVelocity = FVector::ZeroVector;
		SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, false);
		break;
	}
}

void UUEPokemonTestServerComponent::SendServerSnapshot(AUEPokemonCharacter& PokemonCharacter, const FVector& Location, const FVector& Velocity, const FRotator& Rotation, bool bTeleported) const
{
	PokemonCharacter.ApplyServerMoveTarget(Location, Velocity, Rotation, bTeleported);
}

AUEPokemonCharacter* UUEPokemonTestServerComponent::GetPokemonOwner() const
{
	return Cast<AUEPokemonCharacter>(GetOwner());
}

AActor* UUEPokemonTestServerComponent::ResolveFollowTargetActor() const
{
	if (FollowTargetActor)
	{
		return FollowTargetActor;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const APlayerController* PlayerController = World->GetFirstPlayerController();
	return PlayerController ? PlayerController->GetPawn() : nullptr;
}

HHV::PokemonAI::CompanionContext UUEPokemonTestServerComponent::MakeCompanionContext(const AUEPokemonCharacter& PokemonCharacter, const AActor& CurrentFollowTargetActor, float DeltaSeconds) const
{
	HHV::PokemonAI::CompanionContext Context;
	Context.DeltaSeconds = DeltaSeconds;
	Context.PokemonLocation = ToServerVec3(ServerSimulatedLocation);
	Context.OwnerLocation = ToServerVec3(CurrentFollowTargetActor.GetActorLocation());
	Context.OwnerYawDegrees = GetFollowTargetYawDegrees(CurrentFollowTargetActor);
	Context.ActionRequest = HHV::PokemonAI::RequestedAction::FollowOwner;
	Context.ServerMap = bServerMapLoaded ? &ServerMapRuntime : nullptr;
	Context.Agent = MakeAgentSettings(PokemonCharacter);
	return Context;
}

HHV::Map::AgentSettings UUEPokemonTestServerComponent::MakeAgentSettings(const AUEPokemonCharacter& PokemonCharacter) const
{
	HHV::Map::AgentSettings Agent;

	if (const UCapsuleComponent* CapsuleComponent = PokemonCharacter.GetCapsuleComponent())
	{
		Agent.CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
		Agent.CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	}

	if (const UCharacterMovementComponent* MovementComponent = PokemonCharacter.GetCharacterMovement())
	{
		Agent.MaxStepHeight = MovementComponent->MaxStepHeight;
		Agent.WalkableFloorAngleDegrees = MovementComponent->GetWalkableFloorAngle();
	}

	return Agent;
}

FString UUEPokemonTestServerComponent::ResolveServerMapFilePath() const
{
	if (!ServerMapFilePath.IsEmpty())
	{
		return ServerMapFilePath;
	}

	if (!bTryLoadDefaultServerMap)
	{
		return FString();
	}

	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("ServerMaps"),
		TEXT("PlayerTestLevel.hhvservermap")
	);
}

float UUEPokemonTestServerComponent::GetFollowTargetYawDegrees(const AActor& CurrentFollowTargetActor) const
{
	if (const APawn* Pawn = Cast<APawn>(&CurrentFollowTargetActor))
	{
		if (const AController* Controller = Pawn->GetController())
		{
			return Controller->GetControlRotation().Yaw;
		}
	}

	return CurrentFollowTargetActor.GetActorRotation().Yaw;
}

HHV::Map::Vec3 UUEPokemonTestServerComponent::ToServerVec3(const FVector& Vector)
{
	return HHV::Map::Vec3{
		static_cast<float>(Vector.X),
		static_cast<float>(Vector.Y),
		static_cast<float>(Vector.Z)
	};
}

FVector UUEPokemonTestServerComponent::ToUnrealVector(const HHV::Map::Vec3& Vector)
{
	return FVector(Vector.X, Vector.Y, Vector.Z);
}
