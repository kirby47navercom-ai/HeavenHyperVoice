#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UEGachaPool.h"
#include "UEGachaMachine.generated.h"

class UCameraComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EUEGachaState : uint8 { Ready, Turning, Dispensing, Result };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUEGachaRewardRevealed, const FUEGachaEntry&, Entry);

/** 기계 메시와 캡슐 배치는 BP_GachaMachine에서 수정한다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEGachaMachine : public AActor
{
	GENERATED_BODY()
public:
	AUEGachaMachine();
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gacha") TObjectPtr<UUEGachaPool> Pool;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gacha") TArray<TObjectPtr<UStaticMesh>> BallMeshes;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gacha") TObjectPtr<USceneComponent> HandlePivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gacha") TObjectPtr<UStaticMeshComponent> RewardBall;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gacha") TObjectPtr<UPointLightComponent> StageLight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gacha") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(EditDefaultsOnly, Category="Gacha|Chamber") FVector ChamberCenter = FVector(0, 0, 235);
	UPROPERTY(EditDefaultsOnly, Category="Gacha|Chamber", meta=(ClampMin="20")) float ChamberRadius = 76;
	UPROPERTY(EditDefaultsOnly, Category="Gacha|Chamber", meta=(ClampMin="1")) float CapsuleRadius = 11;
	UPROPERTY(BlueprintReadOnly, Category="Gacha") EUEGachaState State = EUEGachaState::Ready;
	UPROPERTY(BlueprintReadOnly, Category="Gacha") int32 CompletedTurns = 0;
	UPROPERTY(BlueprintReadOnly, Category="Gacha") float TurnDegrees = 0;
	UPROPERTY(BlueprintReadOnly, Category="Gacha") FText StatusMessage;
	UPROPERTY(BlueprintReadOnly, Category="Gacha") FLinearColor RevealColor;
	UPROPERTY(BlueprintAssignable, Category="Gacha") FUEGachaRewardRevealed OnRewardRevealed;
	UFUNCTION(BlueprintCallable, Category="Gacha") void ResetDraw();
	const FUEGachaEntry& GetResult() const { return SelectedEntry; }
	void TurnHandle(float ClockwiseDegrees);
	bool CanTurn() const { return State == EUEGachaState::Ready || State == EUEGachaState::Turning; }
protected:
	virtual void BeginPlay() override;
private:
	void UpdateStage();
	void SimulateCapsules(float Seconds);
	FUEGachaEntry SelectedEntry;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Capsules;
	TArray<FVector> Velocities;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> StageMaterials;
	float DispenseElapsed = 0;
	float Pulse = 0;
};
