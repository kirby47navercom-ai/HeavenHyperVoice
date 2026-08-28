#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UEPlayerMovementSyncComponent.generated.h"

class AUEPlayerCharacter;

struct FUEPlayerMovementPacket
{
	uint32 Sequence = 0;
	FVector ClientPosition = FVector::ZeroVector;
	FVector ClientVelocity = FVector::ZeroVector;
	FRotator ActorRotation = FRotator::ZeroRotator;
};

struct FUEPlayerMovementHistoryEntry
{
	FUEPlayerMovementPacket Packet;
	FVector ReportedPosition = FVector::ZeroVector;
	FRotator ReportedRotation = FRotator::ZeroRotator;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEPlayerMovementSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEPlayerMovementSyncComponent();

	FUEPlayerMovementPacket CaptureMovementPacket();
	void HandleServerEnterAck(uint64 EntityId, const FVector& ServerPosition, const FRotator& ServerRotation);
	void HandleServerCorrection(uint32 Sequence, const FVector2D& ServerPositionXY, float ServerFacing);
	void HandleServerMovementResult(uint32 AckSequence, const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync", meta = (ClampMin = "0.0"))
	float ServerCorrectionTolerance = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync", meta = (ClampMin = "0.0"))
	float HardCorrectionDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync", meta = (ClampMin = "1"))
	int32 MaxMoveHistoryEntries = 180;

private:
	FUEPlayerMovementPacket BuildMovementPacket();
	void RecordMovementPacket(const FUEPlayerMovementPacket& MovementPacket);
	void PruneMoveHistory(int32 LastConfirmedIndex);
	int32 FindMoveHistoryIndex(uint32 Sequence) const;
	AUEPlayerCharacter* GetPlayerCharacter() const;
	void SaveLastValidatedServerState(const FVector& ServerPosition, const FRotator& ServerRotation);

	TWeakObjectPtr<AUEPlayerCharacter> CachedPlayerCharacter;
	uint32 NextMoveSequence = 1;
	TArray<FUEPlayerMovementHistoryEntry> MoveHistory;
	bool bLastValidatedServerStateValid = false;
	FVector LastValidatedServerPosition = FVector::ZeroVector;
	FRotator LastValidatedServerRotation = FRotator::ZeroRotator;
};
