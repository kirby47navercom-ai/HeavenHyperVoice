#include "UEPlayerMovementSyncComponent.h"

#include "../../Character/UEPlayerCharacter.h"

UUEPlayerMovementSyncComponent::UUEPlayerMovementSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UUEPlayerMovementSyncComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedPlayerCharacter = Cast<AUEPlayerCharacter>(GetOwner());
	if (AUEPlayerCharacter* PlayerCharacter = CachedPlayerCharacter.Get())
	{
		SaveLastValidatedServerState(PlayerCharacter->GetActorLocation(), PlayerCharacter->GetActorRotation());
	}
}

void UUEPlayerMovementSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CachedPlayerCharacter = nullptr;
	MoveHistory.Reset();

	Super::EndPlay(EndPlayReason);
}

FUEPlayerMovementPacket UUEPlayerMovementSyncComponent::CaptureMovementPacket()
{
	const FUEPlayerMovementPacket MovementPacket = BuildMovementPacket();
	if (MovementPacket.Sequence != 0)
	{
		RecordMovementPacket(MovementPacket);
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
	SaveLastValidatedServerState(ServerPosition, ServerRotation);

	UE_LOG(LogTemp, Display,
		TEXT("PlayerMovementSync: entered field as entity %llu at (%.0f, %.0f)"),
		EntityId,
		ServerPosition.X,
		ServerPosition.Y);
}

void UUEPlayerMovementSyncComponent::HandleServerCorrection(
	uint32 Sequence,
	const FVector2D& ServerPositionXY,
	float ServerFacing)
{
	const int32 HistoryIndex = FindMoveHistoryIndex(Sequence);
	if (HistoryIndex == INDEX_NONE)
	{
		return;
	}

	const FUEPlayerMovementHistoryEntry& HistoryEntry = MoveHistory[HistoryIndex];

	FVector ServerPosition = HistoryEntry.ReportedPosition;
	ServerPosition.X = ServerPositionXY.X;
	ServerPosition.Y = ServerPositionXY.Y;

	FRotator ServerRotation = HistoryEntry.ReportedRotation;
	ServerRotation.Yaw = ServerFacing;

	HandleServerMovementResult(Sequence, ServerPosition, FVector::ZeroVector, ServerRotation);
}

void UUEPlayerMovementSyncComponent::HandleServerMovementResult(
	uint32 AckSequence,
	const FVector& ServerPosition,
	const FVector& ServerVelocity,
	const FRotator& ServerRotation)
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
		SaveLastValidatedServerState(ServerPosition, ServerRotation);
		return;
	}

	const bool bUseHardCorrection = CorrectionDistance >= HardCorrectionDistance;
	PlayerCharacter->ApplyServerMovementCorrection(
		ServerPosition,
		ServerVelocity,
		ServerRotation,
		bUseHardCorrection);
	PruneMoveHistory(HistoryIndex);
	SaveLastValidatedServerState(ServerPosition, ServerRotation);
}

FUEPlayerMovementPacket UUEPlayerMovementSyncComponent::BuildMovementPacket()
{
	FUEPlayerMovementPacket MovementPacket;

	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return MovementPacket;
	}

	MovementPacket.Sequence = NextMoveSequence++;
	MovementPacket.ClientPosition = PlayerCharacter->GetActorLocation();
	MovementPacket.ClientVelocity = PlayerCharacter->GetVelocity();
	MovementPacket.ActorRotation = PlayerCharacter->GetActorRotation();
	return MovementPacket;
}

void UUEPlayerMovementSyncComponent::RecordMovementPacket(const FUEPlayerMovementPacket& MovementPacket)
{
	FUEPlayerMovementHistoryEntry HistoryEntry;
	HistoryEntry.Packet = MovementPacket;
	HistoryEntry.ReportedPosition = MovementPacket.ClientPosition;
	HistoryEntry.ReportedRotation = MovementPacket.ActorRotation;
	MoveHistory.Add(MoveTemp(HistoryEntry));

	while (MoveHistory.Num() > MaxMoveHistoryEntries)
	{
		MoveHistory.RemoveAt(0, 1, EAllowShrinking::No);
	}
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

void UUEPlayerMovementSyncComponent::SaveLastValidatedServerState(
	const FVector& ServerPosition,
	const FRotator& ServerRotation)
{
	LastValidatedServerPosition = ServerPosition;
	LastValidatedServerRotation = ServerRotation;
	bLastValidatedServerStateValid = true;
}
