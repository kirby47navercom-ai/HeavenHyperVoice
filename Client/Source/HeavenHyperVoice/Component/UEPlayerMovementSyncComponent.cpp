// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPlayerMovementSyncComponent.h"

#include "../Character/UEPlayerCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"

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
	// TODO: Serialize and send this movement-state packet to the external C++ validation server.
	(void)MovementPacket;
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
