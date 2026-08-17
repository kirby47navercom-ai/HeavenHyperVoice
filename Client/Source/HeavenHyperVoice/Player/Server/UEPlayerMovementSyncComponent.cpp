// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPlayerMovementSyncComponent.h"

#include "../../Character/UEPlayerCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Paths.h"

UUEPlayerMovementSyncComponent::UUEPlayerMovementSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UUEPlayerMovementSyncComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedPlayerCharacter = Cast<AUEPlayerCharacter>(GetOwner());
	AUEPlayerCharacter* PlayerCharacter = CachedPlayerCharacter.Get();
	if (!PlayerCharacter)
	{
		return;
	}

	PlayerCharacter->OnCharacterMovementUpdated.AddDynamic(this, &ThisClass::HandleCharacterMovementUpdated);
	SaveLastValidatedServerState(PlayerCharacter->GetActorLocation(), PlayerCharacter->GetVelocity(), PlayerCharacter->GetActorRotation());
	TryLoadServerMap();
}

void UUEPlayerMovementSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter())
	{
		PlayerCharacter->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleCharacterMovementUpdated);
	}

	Super::EndPlay(EndPlayReason);
}

FUEPlayerMovementPacket UUEPlayerMovementSyncComponent::BuildMovementPacket(float DeltaSeconds)
{
	FUEPlayerMovementPacket MovementPacket;

	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return MovementPacket;
	}

	MovementPacket.Sequence = NextMoveSequence++;
	MovementPacket.DeltaSeconds = DeltaSeconds;
	MovementPacket.MoveInput = PlayerCharacter->GetMovementInput();
	MovementPacket.ClientPosition = PlayerCharacter->GetActorLocation();
	MovementPacket.ClientVelocity = PlayerCharacter->GetVelocity();
	MovementPacket.ControlRotation = PlayerCharacter->GetControlRotation();
	MovementPacket.ActorRotation = PlayerCharacter->GetActorRotation();

	return MovementPacket;
}

void UUEPlayerMovementSyncComponent::RecordMovementPacket(const FUEPlayerMovementPacket& MovementPacket)
{
	FUEPlayerMovementHistoryEntry HistoryEntry;
	HistoryEntry.Packet = MovementPacket;
	HistoryEntry.ReportedPosition = MovementPacket.ClientPosition;
	HistoryEntry.ReportedVelocity = MovementPacket.ClientVelocity;
	HistoryEntry.ReportedRotation = MovementPacket.ActorRotation;
	MoveHistory.Add(MoveTemp(HistoryEntry));

	// Keep only recent reported states so latency spikes cannot grow memory forever.
	while (MoveHistory.Num() > MaxMoveHistoryEntries)
	{
		MoveHistory.RemoveAt(0, 1, EAllowShrinking::No);
	}
}

void UUEPlayerMovementSyncComponent::SendMovementPacketToServer(const FUEPlayerMovementPacket& MovementPacket)
{
	if (bEnableLocalServerValidation)
	{
		ValidateMovementPacketOnLocalServer(MovementPacket);
		return;
	}

	// TODO: Serialize and send this movement-state packet to the external C++ validation server.
	(void)MovementPacket;
}

void UUEPlayerMovementSyncComponent::TryLoadServerMap()
{
	const FString MapFilePath = ResolveServerMapFilePath();
	if (MapFilePath.IsEmpty() || !FPaths::FileExists(MapFilePath))
	{
		bServerMapLoaded = false;
		UE_LOG(LogTemp, Warning, TEXT("PlayerMovementSync: server map not found: %s"), *MapFilePath);
		return;
	}

	bServerMapLoaded = ServerMapRuntime.LoadFromFile(TCHAR_TO_UTF8(*MapFilePath));
	UE_LOG(
		LogTemp,
		Display,
		TEXT("PlayerMovementSync: server map %s: %s"),
		bServerMapLoaded ? TEXT("loaded") : TEXT("failed"),
		*MapFilePath
	);
}

void UUEPlayerMovementSyncComponent::ValidateMovementPacketOnLocalServer(const FUEPlayerMovementPacket& MovementPacket)
{
	FVector ServerPosition = MovementPacket.ClientPosition;
	FVector ServerVelocity = MovementPacket.ClientVelocity;
	FRotator ServerRotation = MovementPacket.ActorRotation;

	BuildLocalServerMovementResult(MovementPacket, ServerPosition, ServerVelocity, ServerRotation);
	HandleServerMovementResult(MovementPacket.Sequence, ServerPosition, ServerVelocity, ServerRotation);
}

bool UUEPlayerMovementSyncComponent::BuildLocalServerMovementResult(const FUEPlayerMovementPacket& MovementPacket, FVector& OutServerPosition, FVector& OutServerVelocity, FRotator& OutServerRotation)
{
	if (!bServerMapLoaded)
	{
		SaveLastValidatedServerState(MovementPacket.ClientPosition, MovementPacket.ClientVelocity, MovementPacket.ActorRotation);
		OutServerPosition = MovementPacket.ClientPosition;
		OutServerVelocity = MovementPacket.ClientVelocity;
		OutServerRotation = MovementPacket.ActorRotation;
		return true;
	}

	const HHV::Map::AgentSettings Agent = MakeAgentSettings();
	const HHV::Map::FloorSample Floor = ServerMapRuntime.SampleFloor(
		static_cast<float>(MovementPacket.ClientPosition.X),
		static_cast<float>(MovementPacket.ClientPosition.Y),
		Agent.WalkableFloorAngleDegrees
	);
	const HHV::Map::Vec3 ServerCapsuleLocation{
		static_cast<float>(MovementPacket.ClientPosition.X),
		static_cast<float>(MovementPacket.ClientPosition.Y),
		Floor.FloorZ + Agent.CapsuleHalfHeight
	};
	const bool bHasGroundAtXY = Floor.bHit;
	const bool bBlockedByWall = bHasGroundAtXY && ServerMapRuntime.IsBlockedByWall(ServerCapsuleLocation, Agent);
	if (bHasGroundAtXY && !bBlockedByWall)
	{
		// Valid movement is approved exactly as the client reported it.
		OutServerPosition = MovementPacket.ClientPosition;
		OutServerVelocity = MovementPacket.ClientVelocity;
		OutServerRotation = MovementPacket.ActorRotation;
		SaveLastValidatedServerState(OutServerPosition, OutServerVelocity, OutServerRotation);
		return true;
	}

	// If the reported state is outside the server map, keep the last server-approved state.
	OutServerPosition = bLastValidatedServerStateValid ? LastValidatedServerPosition : MovementPacket.ClientPosition;
	OutServerVelocity = FVector::ZeroVector;
	OutServerRotation = bLastValidatedServerStateValid ? LastValidatedServerRotation : MovementPacket.ActorRotation;
	return false;
}

FString UUEPlayerMovementSyncComponent::ResolveServerMapFilePath() const
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

HHV::Map::AgentSettings UUEPlayerMovementSyncComponent::MakeAgentSettings() const
{
	HHV::Map::AgentSettings Agent;
	const AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return Agent;
	}

	if (const UCapsuleComponent* CapsuleComponent = PlayerCharacter->GetCapsuleComponent())
	{
		Agent.CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
		Agent.CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	}

	if (const UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
	{
		Agent.MaxStepHeight = MovementComponent->MaxStepHeight;
		Agent.WalkableFloorAngleDegrees = MovementComponent->GetWalkableFloorAngle();
	}

	return Agent;
}

void UUEPlayerMovementSyncComponent::HandleServerMovementResult(uint32 AckSequence, const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation)
{
	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return;
	}

	const int32 HistoryIndex = FindMoveHistoryIndex(AckSequence);
	if (HistoryIndex == INDEX_NONE)
	{
		return;
	}

	const FUEPlayerMovementHistoryEntry& HistoryEntry = MoveHistory[HistoryIndex];
	const float CorrectionDistance = FVector::Dist(HistoryEntry.ReportedPosition, ServerPosition);
	if (CorrectionDistance <= ServerCorrectionTolerance)
	{
		PruneMoveHistory(HistoryIndex);
		return;
	}

	// Server validation wins when the reported state is outside the allowed error range.
	const bool bUseHardCorrection = CorrectionDistance >= HardCorrectionDistance;
	PlayerCharacter->ApplyServerMovementCorrection(ServerPosition, ServerVelocity, ServerRotation, bUseHardCorrection);
	PruneMoveHistory(HistoryIndex);
}

void UUEPlayerMovementSyncComponent::PruneMoveHistory(int32 LastConfirmedIndex)
{
	if (LastConfirmedIndex >= 0)
	{
		MoveHistory.RemoveAt(0, LastConfirmedIndex + 1, EAllowShrinking::No);
	}
}

int32 UUEPlayerMovementSyncComponent::FindMoveHistoryIndex(uint32 Sequence) const
{
	for (int32 Index = 0; Index < MoveHistory.Num(); ++Index)
	{
		if (MoveHistory[Index].Packet.Sequence == Sequence)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

AUEPlayerCharacter* UUEPlayerMovementSyncComponent::GetPlayerCharacter() const
{
	if (CachedPlayerCharacter.IsValid())
	{
		return CachedPlayerCharacter.Get();
	}

	return Cast<AUEPlayerCharacter>(GetOwner());
}

void UUEPlayerMovementSyncComponent::SaveLastValidatedServerState(const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation)
{
	LastValidatedServerPosition = ServerPosition;
	LastValidatedServerVelocity = ServerVelocity;
	LastValidatedServerRotation = ServerRotation;
	bLastValidatedServerStateValid = true;
}

HHV::Map::Vec3 UUEPlayerMovementSyncComponent::ToServerVec3(const FVector& Vector)
{
	return HHV::Map::Vec3{
		static_cast<float>(Vector.X),
		static_cast<float>(Vector.Y),
		static_cast<float>(Vector.Z)
	};
}

FVector UUEPlayerMovementSyncComponent::ToUnrealVector(const HHV::Map::Vec3& Vector)
{
	return FVector(Vector.X, Vector.Y, Vector.Z);
}

void UUEPlayerMovementSyncComponent::HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	(void)OldLocation;
	(void)OldVelocity;

	const AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter || !PlayerCharacter->IsLocallyControlled())
	{
		return;
	}

	const FUEPlayerMovementPacket MovementPacket = BuildMovementPacket(DeltaSeconds);
	RecordMovementPacket(MovementPacket);
	SendMovementPacketToServer(MovementPacket);
}
