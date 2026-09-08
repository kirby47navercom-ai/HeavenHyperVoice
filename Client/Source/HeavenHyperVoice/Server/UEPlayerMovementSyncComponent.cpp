#include "UEPlayerMovementSyncComponent.h"

#include "../Character/UEPlayerCharacter.h"

UUEPlayerMovementSyncComponent::UUEPlayerMovementSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UUEPlayerMovementSyncComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedPlayerCharacter = Cast<AUEPlayerCharacter>(GetOwner());
}

void UUEPlayerMovementSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CachedPlayerCharacter = nullptr;
	MoveHistory.Reset();

	Super::EndPlay(EndPlayReason);
}

FUEPlayerMovementPacket UUEPlayerMovementSyncComponent::CaptureMovementPacket()
{
	FUEPlayerMovementPacket MovementPacket;

	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return MovementPacket;
	}

	MovementPacket.Sequence = NextMoveSequence++;
	MovementPacket.ClientPosition = PlayerCharacter->GetActorLocation();
	MovementPacket.ActorRotation = PlayerCharacter->GetActorRotation();

	MoveHistory.Add(MovementPacket);
	while (MoveHistory.Num() > MaxMoveHistoryEntries)
	{
		MoveHistory.RemoveAt(0, 1, EAllowShrinking::No);
	}
	return MovementPacket;
}

void UUEPlayerMovementSyncComponent::HandleServerEnterAck(
	uint64 EntityId,
	const FVector& ServerPosition,
	const FRotator& ServerRotation)
{
	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return;
	}

	PlayerCharacter->ApplyServerMovementCorrection(
		ServerPosition,
		FVector::ZeroVector,
		ServerRotation,
		/*bUseHardCorrection=*/true);

	UE_LOG(LogTemp, Display,
		TEXT("PlayerMovementSync: entered field as entity %llu at (%.0f, %.0f)"),
		EntityId,
		ServerPosition.X,
		ServerPosition.Y);
}

void UUEPlayerMovementSyncComponent::HandleServerCorrection(
	uint32 Sequence,
	const FVector& ServerPosition,
	float ServerFacing)
{
	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return;
	}

	const int32 HistoryIndex = FindMoveHistoryIndex(Sequence);
	if (HistoryIndex == INDEX_NONE)
	{
		return;
	}
	const FVector Error = FVector(ServerPosition.X - MoveHistory[HistoryIndex].ClientPosition.X,
		ServerPosition.Y - MoveHistory[HistoryIndex].ClientPosition.Y, 0.0);
	const float CorrectionDistance = Error.Size2D();
	MoveHistory.RemoveAt(0, HistoryIndex + 1, EAllowShrinking::No);

	if (CorrectionDistance <= ServerCorrectionTolerance)
	{
		return;
	}

	const bool bHard = CorrectionDistance >= HardCorrectionDistance;
	const FVector Before = PlayerCharacter->GetActorLocation();
	FRotator Rotation = PlayerCharacter->GetActorRotation();
	if (bHard)
	{
		Rotation.Yaw = ServerFacing;
	}
	// The protocol validates XY, not jumping/falling. Preserve vertical physics
	// and movement performed since the acknowledged packet for ordinary corrections.
	PlayerCharacter->ApplyServerMovementCorrection(bHard ? ServerPosition : Before + Error,
		bHard ? FVector::ZeroVector : PlayerCharacter->GetVelocity(), Rotation, bHard);
	if (bHard)
	{
		MoveHistory.Reset();
	}
	else
	{
		// Later replies refer to packets sent before this adjustment. Rebase the
		// remaining history so the same displacement is not applied twice.
		FVector Applied = PlayerCharacter->GetActorLocation() - Before;
		Applied.Z = 0.0;
		for (FUEPlayerMovementPacket& Pending : MoveHistory)
		{
			Pending.ClientPosition += Applied;
		}
	}
}

int32 UUEPlayerMovementSyncComponent::FindMoveHistoryIndex(uint32 Sequence) const
{
	for (int32 Index = 0; Index < MoveHistory.Num(); ++Index)
	{
		if (MoveHistory[Index].Sequence == Sequence)
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

