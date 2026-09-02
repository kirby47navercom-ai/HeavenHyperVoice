#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UEPlayerMovementSyncComponent.generated.h"

class AUEPlayerCharacter;

struct FUEPlayerMovementPacket
{
	uint32 Sequence = 0;
	FVector ClientPosition = FVector::ZeroVector;
	FRotator ActorRotation = FRotator::ZeroRotator;
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
	int32 FindMoveHistoryIndex(uint32 Sequence) const;
	AUEPlayerCharacter* GetPlayerCharacter() const;

	TWeakObjectPtr<AUEPlayerCharacter> CachedPlayerCharacter;
	uint32 NextMoveSequence = 1;

	// 서버가 보정을 보낼 때 그 Sequence 시점에 내가 어디였는지 되짚는 데 쓴다.
	TArray<FUEPlayerMovementPacket> MoveHistory;
};
