// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPokemonServerComponent.h"

#include "../UEPokemonCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Paths.h"

namespace
{
	int32 GNextPokemonServerId = 1;

	FVector ToLocalUnrealVector(const HHV::Map::Vec3& Vector)
	{
		return FVector(Vector.X, Vector.Y, Vector.Z);
	}

	bool TryFindNextPathTarget(const HHV::PokemonAI::Command& Command, const FVector& CurrentTarget, float AcceptanceRadius, FVector& OutNextTarget)
	{
		const std::vector<HHV::Map::Vec3>& PathPoints = Command.PathPoints;
		if (PathPoints.size() <= 1)
		{
			return false;
		}

		int32 ClosestTargetIndex = INDEX_NONE;
		float ClosestTargetDistanceSquared = BIG_NUMBER;
		for (int32 Index = 0; Index < static_cast<int32>(PathPoints.size()); ++Index)
		{
			const FVector PathPoint = ToLocalUnrealVector(PathPoints[static_cast<std::size_t>(Index)]);
			const float DistanceSquared = FVector::DistSquared(PathPoint, CurrentTarget);
			if (DistanceSquared < ClosestTargetDistanceSquared)
			{
				ClosestTargetDistanceSquared = DistanceSquared;
				ClosestTargetIndex = Index;
			}
		}

		if (ClosestTargetIndex == INDEX_NONE || ClosestTargetIndex >= static_cast<int32>(PathPoints.size()) - 1)
		{
			return false;
		}

		const float MinimumAdvanceDistanceSquared = FMath::Square(FMath::Max(AcceptanceRadius, 1.0f));
		for (int32 Index = ClosestTargetIndex + 1; Index < static_cast<int32>(PathPoints.size()); ++Index)
		{
			const FVector PathPoint = ToLocalUnrealVector(PathPoints[static_cast<std::size_t>(Index)]);
			if (FVector::DistSquared2D(PathPoint, CurrentTarget) > MinimumAdvanceDistanceSquared)
			{
				OutNextTarget = PathPoint;
				return true;
			}
		}

		return false;
	}
}

UUEPokemonServerComponent::UUEPokemonServerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UUEPokemonServerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ServerPokemonId <= 0)
	{
		ServerPokemonId = GNextPokemonServerId++;
	}

	if (const AUEPokemonCharacter* PokemonCharacter = GetPokemonOwner())
	{
		ServerSimulatedLocation = PokemonCharacter->GetActorLocation();
		ServerSimulatedRotation = PokemonCharacter->GetActorRotation();
		if (ServerMaxHP <= 0.0f)
		{
			ServerMaxHP = PokemonCharacter->GetMaxHP();
			ServerCurrentHP = PokemonCharacter->GetCurrentHP();
		}
	}

	const int32 Seed = WanderRandomSeed != 0 ? WanderRandomSeed : ServerPokemonId * 7919 + 104729;
	WildBrain.SetRandomSeed(static_cast<std::uint32_t>(Seed));

	TryLoadServerMap();
}

void UUEPokemonServerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnableServer)
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

void UUEPokemonServerComponent::SetFollowTargetActor(AActor* NewFollowTargetActor)
{
	FollowTargetActor = NewFollowTargetActor;
}

void UUEPokemonServerComponent::SetServerSimulationEnabled(bool bNewEnabled)
{
	bEnableServer = bNewEnabled;
	SetComponentTickEnabled(bEnableServer);
}

void UUEPokemonServerComponent::SetServerSimulationMode(EUEPokemonServerSimulationMode NewSimulationMode)
{
	if (SimulationMode == NewSimulationMode)
	{
		return;
	}

	SimulationMode = NewSimulationMode;
	ResetWildState();
}

void UUEPokemonServerComponent::InitializeServerRuntimePokemon(int32 RuntimePokemonId, int32 PokemonInstanceId, float CurrentHP, float MaxHP)
{
	if (RuntimePokemonId > 0)
	{
		ServerPokemonId = RuntimePokemonId;
		const int32 Seed = WanderRandomSeed != 0 ? WanderRandomSeed : ServerPokemonId * 7919 + 104729;
		WildBrain.SetRandomSeed(static_cast<std::uint32_t>(Seed));
	}

	ServerPokemonInstanceId = FMath::Max(PokemonInstanceId, 0);
	if (MaxHP > 0.0f)
	{
		ServerMaxHP = FMath::Max(MaxHP, 1.0f);
		ServerCurrentHP = FMath::Clamp(CurrentHP, 0.0f, ServerMaxHP);
		return;
	}

	if (const AUEPokemonCharacter* PokemonCharacter = GetPokemonOwner())
	{
		ServerMaxHP = PokemonCharacter->GetMaxHP();
		ServerCurrentHP = PokemonCharacter->GetCurrentHP();
	}
}

void UUEPokemonServerComponent::SendServerAnimationEvent(EUEPokemonAnimationEvent AnimationEvent, EUEPokemonAnimationState AnimationState, float EventDurationSeconds)
{
	AUEPokemonCharacter* PokemonCharacter = GetPokemonOwner();
	if (!PokemonCharacter)
	{
		return;
	}

	ServerSimulatedLocation = PokemonCharacter->GetActorLocation();
	ServerSimulatedRotation = PokemonCharacter->GetActorRotation();
	ServerSimulatedVelocity = FVector::ZeroVector;

	const float CurrentServerTimeSeconds = GetServerTimeSeconds();
	if (EventDurationSeconds > 0.0f)
	{
		bHasForcedAnimationState = true;
		ForcedAnimationState = AnimationState;
		ForcedAnimationStateEndServerTimeSeconds = CurrentServerTimeSeconds + EventDurationSeconds;
	}

	SendServerSnapshot(
		*PokemonCharacter,
		ServerSimulatedLocation,
		ServerSimulatedVelocity,
		ServerSimulatedRotation,
		false,
		AnimationState,
		AnimationEvent,
		EventDurationSeconds
	);
}

void UUEPokemonServerComponent::TryLoadServerMap()
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
		TEXT("PokemonServer: server map %s: %s"),
		bServerMapLoaded ? TEXT("loaded") : TEXT("failed"),
		*MapFilePath
	);
}

void UUEPokemonServerComponent::RunServerSimulationTick(float DeltaSeconds)
{
	AUEPokemonCharacter* PokemonCharacter = GetPokemonOwner();
	if (!PokemonCharacter)
	{
		return;
	}

	if (SimulationMode == EUEPokemonServerSimulationMode::Wander)
	{
		RunWanderSimulationTick(*PokemonCharacter, DeltaSeconds);
		return;
	}

	RunFollowSimulationTick(*PokemonCharacter, DeltaSeconds);
}

void UUEPokemonServerComponent::RunFollowSimulationTick(AUEPokemonCharacter& PokemonCharacter, float DeltaSeconds)
{
	const AActor* CurrentFollowTargetActor = ResolveFollowTargetActor();
	if (!CurrentFollowTargetActor)
	{
		return;
	}

	const HHV::PokemonAI::OwnContext Context = MakeOwnContext(PokemonCharacter, *CurrentFollowTargetActor, DeltaSeconds);
	const HHV::PokemonAI::Command Command = ServerBrain.Tick(Context);
	ApplyServerCommand(PokemonCharacter, Command, DeltaSeconds);
}

void UUEPokemonServerComponent::RunWanderSimulationTick(AUEPokemonCharacter& PokemonCharacter, float DeltaSeconds)
{
	const HHV::PokemonAI::WildContext Context = MakeWildContext(PokemonCharacter, DeltaSeconds);
	const HHV::PokemonAI::Command Command = WildBrain.Tick(Context);
	ApplyServerCommand(PokemonCharacter, Command, DeltaSeconds);
}

void UUEPokemonServerComponent::ApplyServerCommand(AUEPokemonCharacter& PokemonCharacter, const HHV::PokemonAI::Command& Command, float DeltaSeconds)
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
		const float MoveSpeed = ResolveServerMoveSpeed(PokemonCharacter);
		if (DistanceToTarget <= AcceptanceRadius)
		{
			ServerSimulatedLocation = TargetLocation;
			FVector NextPathTarget;
			if (TryFindNextPathTarget(Command, TargetLocation, AcceptanceRadius, NextPathTarget))
			{
				const FVector ToNextTarget = NextPathTarget - ServerSimulatedLocation;
				const FVector MoveDirection = ToNextTarget.GetSafeNormal();
				ServerSimulatedVelocity = MoveDirection * MoveSpeed;
				if (!MoveDirection.IsNearlyZero())
				{
					ServerSimulatedRotation = MoveDirection.ToOrientationRotator();
				}
			}
			else
			{
				ServerSimulatedVelocity = FVector::ZeroVector;
			}
		}
		else
		{
			const FVector MoveDirection = ToTarget / DistanceToTarget;
			const float MoveDistance = FMath::Min(MoveSpeed * DeltaSeconds, DistanceToTarget);
			ServerSimulatedLocation += MoveDirection * MoveDistance;
			ServerSimulatedVelocity = MoveDirection * MoveSpeed;
			ServerSimulatedRotation = MoveDirection.ToOrientationRotator();
		}

		// The local server sends the same movement snapshot the real server will send later.
		const EUEPokemonAnimationState AnimationState = ServerSimulatedVelocity.IsNearlyZero()
			? EUEPokemonAnimationState::Idle
			: EUEPokemonAnimationState::Moving;
		SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, false, ResolveAnimationState(AnimationState), EUEPokemonAnimationEvent::None, 0.0f);
		break;
	}
	case CommandType::Teleport:
		ServerSimulatedLocation = ToUnrealVector(Command.TargetLocation);
		ServerSimulatedVelocity = FVector::ZeroVector;
		SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, true, ResolveAnimationState(EUEPokemonAnimationState::Idle), EUEPokemonAnimationEvent::None, 0.0f);
		break;
	case CommandType::FaceTarget:
	{
		FVector ToTarget = ToUnrealVector(Command.TargetLocation) - ServerSimulatedLocation;
		ToTarget.Z = 0.0f;
		ServerSimulatedVelocity = FVector::ZeroVector;
		if (!ToTarget.IsNearlyZero())
		{
			ServerSimulatedRotation = ToTarget.ToOrientationRotator();
		}
		const EUEPokemonAnimationState AnimationState = ToTarget.IsNearlyZero()
			? EUEPokemonAnimationState::Idle
			: EUEPokemonAnimationState::Turning;
		SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, false, ResolveAnimationState(AnimationState), EUEPokemonAnimationEvent::None, 0.0f);
		break;
	}
	case CommandType::Stop:
	case CommandType::None:
	default:
		ServerSimulatedVelocity = FVector::ZeroVector;
		SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, false, ResolveAnimationState(EUEPokemonAnimationState::Idle), EUEPokemonAnimationEvent::None, 0.0f);
		break;
	}
}

void UUEPokemonServerComponent::SendServerSnapshot(AUEPokemonCharacter& PokemonCharacter, const FVector& Location, const FVector& Velocity, const FRotator& Rotation, bool bTeleported, EUEPokemonAnimationState AnimationState, EUEPokemonAnimationEvent AnimationEvent, float EventDurationSeconds) const
{
	FUEPokemonServerMoveSnapshot Snapshot;
	Snapshot.PokemonId = ServerPokemonId;
	Snapshot.PokemonInstanceId = ServerPokemonInstanceId;
	Snapshot.SpeciesId = PokemonCharacter.GetPokemonSpeciesId();
	Snapshot.RenderType = PokemonCharacter.GetRenderType();
	Snapshot.MaxHP = ServerMaxHP > 0.0f ? ServerMaxHP : PokemonCharacter.GetMaxHP();
	Snapshot.CurrentHP = ServerMaxHP > 0.0f ? FMath::Clamp(ServerCurrentHP, 0.0f, Snapshot.MaxHP) : PokemonCharacter.GetCurrentHP();
	Snapshot.Location = Location;
	Snapshot.Velocity = Velocity;
	Snapshot.Rotation = Rotation;
	Snapshot.bTeleported = bTeleported;
	Snapshot.AnimationState = AnimationState;
	Snapshot.AnimationEvent = AnimationEvent;
	Snapshot.ServerTimeSeconds = GetServerTimeSeconds();
	Snapshot.EventDurationSeconds = EventDurationSeconds;
	PokemonCharacter.ApplyServerMoveSnapshot(Snapshot);
}

EUEPokemonAnimationState UUEPokemonServerComponent::ResolveAnimationState(EUEPokemonAnimationState FallbackState)
{
	if (!bHasForcedAnimationState)
	{
		return FallbackState;
	}

	if (GetServerTimeSeconds() <= ForcedAnimationStateEndServerTimeSeconds)
	{
		return ForcedAnimationState;
	}

	bHasForcedAnimationState = false;
	return FallbackState;
}

float UUEPokemonServerComponent::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0f;
}

float UUEPokemonServerComponent::ResolveServerMoveSpeed(const AUEPokemonCharacter& PokemonCharacter) const
{
	const float SpeciesMoveSpeed = PokemonCharacter.GetConfiguredMoveSpeed();
	return SpeciesMoveSpeed > 0.0f ? SpeciesMoveSpeed : ServerMoveSpeed;
}

void UUEPokemonServerComponent::SendIdleSnapshot(AUEPokemonCharacter& PokemonCharacter)
{
	ServerSimulatedVelocity = FVector::ZeroVector;
	SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, false, ResolveAnimationState(EUEPokemonAnimationState::Idle), EUEPokemonAnimationEvent::None, 0.0f);
}

void UUEPokemonServerComponent::ResetWildState()
{
	WildBrain.Reset();
}

AUEPokemonCharacter* UUEPokemonServerComponent::GetPokemonOwner() const
{
	return Cast<AUEPokemonCharacter>(GetOwner());
}

AActor* UUEPokemonServerComponent::ResolveFollowTargetActor() const
{
	// 따라갈 대상은 반드시 명시돼야 한다. 예전에는 비어 있으면 첫 플레이어 폰으로
	// 넘어갔는데, 그러면 이 컴포넌트를 단 액터가 **누구든** 조용히 플레이어를
	// 따라온다. 기본값이 bEnableServer=true / FollowOwner 라, 서버가 좌표를
	// 지시하는 야생 포켓몬까지 전부 플레이어에게 몰려와 겹쳐 쌓였다.
	// 동행 포켓몬은 SetFollowTargetActor 로 주인을 직접 넣는다.
	return FollowTargetActor;
}

HHV::PokemonAI::OwnContext UUEPokemonServerComponent::MakeOwnContext(const AUEPokemonCharacter& PokemonCharacter, const AActor& CurrentFollowTargetActor, float DeltaSeconds) const
{
	HHV::PokemonAI::OwnContext Context;
	Context.DeltaSeconds = DeltaSeconds;
	Context.PokemonLocation = ToServerVec3(ServerSimulatedLocation);
	Context.OwnerLocation = ToServerVec3(CurrentFollowTargetActor.GetActorLocation());
	Context.OwnerYawDegrees = GetFollowTargetYawDegrees(CurrentFollowTargetActor);
	Context.ActionRequest = HHV::PokemonAI::RequestedAction::FollowOwner;
	Context.ServerMap = bServerMapLoaded ? &ServerMapRuntime : nullptr;
	Context.Agent = MakeAgentSettings(PokemonCharacter);
	return Context;
}

HHV::PokemonAI::WildContext UUEPokemonServerComponent::MakeWildContext(const AUEPokemonCharacter& PokemonCharacter, float DeltaSeconds) const
{
	HHV::PokemonAI::WildContext Context;
	Context.DeltaSeconds = DeltaSeconds;
	Context.PokemonLocation = ToServerVec3(ServerSimulatedLocation);
	Context.ServerMap = bServerMapLoaded ? &ServerMapRuntime : nullptr;
	Context.Agent = MakeAgentSettings(PokemonCharacter);
	Context.WanderWaitSeconds = WanderWaitSeconds;
	Context.WanderAcceptanceRadius = WanderAcceptanceRadius;
	Context.WanderSearchAttempts = WanderSearchAttempts;
	return Context;
}

HHV::Map::AgentSettings UUEPokemonServerComponent::MakeAgentSettings(const AUEPokemonCharacter& PokemonCharacter) const
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

FString UUEPokemonServerComponent::ResolveServerMapFilePath() const
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

float UUEPokemonServerComponent::GetFollowTargetYawDegrees(const AActor& CurrentFollowTargetActor) const
{
	// Own Pokemon placement follows the character body yaw, not the camera/control yaw.
	return CurrentFollowTargetActor.GetActorRotation().Yaw;
}

HHV::Map::Vec3 UUEPokemonServerComponent::ToServerVec3(const FVector& Vector)
{
	return HHV::Map::Vec3{
		static_cast<float>(Vector.X),
		static_cast<float>(Vector.Y),
		static_cast<float>(Vector.Z)
	};
}

FVector UUEPokemonServerComponent::ToUnrealVector(const HHV::Map::Vec3& Vector)
{
	return FVector(Vector.X, Vector.Y, Vector.Z);
}
