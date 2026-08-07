// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UEPlayerMovementSyncComponent.generated.h"

class AUEPlayerCharacter;

struct FUEPlayerMovementPacket
{
	uint32 Sequence = 0;
	float DeltaSeconds = 0.0f;
	FVector2D MoveInput = FVector2D::ZeroVector;
	FVector ClientPosition = FVector::ZeroVector;
	FVector ClientVelocity = FVector::ZeroVector;
	FRotator ControlRotation = FRotator::ZeroRotator;
	FRotator ActorRotation = FRotator::ZeroRotator;
};

struct FUEPlayerMovementHistoryEntry
{
	FUEPlayerMovementPacket Packet;
	FVector ReportedPosition = FVector::ZeroVector;
	FVector ReportedVelocity = FVector::ZeroVector;
	FRotator ReportedRotation = FRotator::ZeroRotator;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEPlayerMovementSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEPlayerMovementSyncComponent();

	void HandleServerMovementResult(uint32 AckSequence, const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	FUEPlayerMovementPacket BuildMovementPacket(float DeltaSeconds);
	void RecordMovementPacket(const FUEPlayerMovementPacket& MovementPacket);
	void SendMovementPacketToServer(const FUEPlayerMovementPacket& MovementPacket);
	void PruneMoveHistory(int32 LastConfirmedIndex);
	int32 FindMoveHistoryIndex(uint32 Sequence) const;
	AUEPlayerCharacter* GetPlayerCharacter() const;

	UFUNCTION()
	void HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync", meta = (ClampMin = "0.0"))
	float ServerCorrectionTolerance = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync", meta = (ClampMin = "0.0"))
	float HardCorrectionDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync", meta = (ClampMin = "1"))
	int32 MaxMoveHistoryEntries = 180;

private:
	TWeakObjectPtr<AUEPlayerCharacter> CachedPlayerCharacter;
	uint32 NextMoveSequence = 1;
	TArray<FUEPlayerMovementHistoryEntry> MoveHistory;
};
