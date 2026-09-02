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
	const FVector2D& ServerPositionXY,
	float ServerFacing)
{
	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return;
	}

	// 서버는 고친 좌표만 보낸다. 높이는 그 Sequence 를 보낼 때 내가 있던 높이다.
	const int32 HistoryIndex = FindMoveHistoryIndex(Sequence);
	if (HistoryIndex == INDEX_NONE)
	{
		return;
	}
	const FUEPlayerMovementPacket& Sent = MoveHistory[HistoryIndex];

	FVector ServerPosition = Sent.ClientPosition;
	ServerPosition.X = ServerPositionXY.X;
	ServerPosition.Y = ServerPositionXY.Y;

	FRotator ServerRotation = Sent.ActorRotation;
	ServerRotation.Yaw = ServerFacing;

	// 확인된 것까지는 다시 볼 일이 없다.
	const float CorrectionDistance = FVector::Dist(Sent.ClientPosition, ServerPosition);
	MoveHistory.RemoveAt(0, HistoryIndex + 1, EAllowShrinking::No);

	if (CorrectionDistance <= ServerCorrectionTolerance)
	{
		return;
	}

	PlayerCharacter->ApplyServerMovementCorrection(
		ServerPosition,
		FVector::ZeroVector,
		ServerRotation,
		/*bUseHardCorrection=*/CorrectionDistance >= HardCorrectionDistance);
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

