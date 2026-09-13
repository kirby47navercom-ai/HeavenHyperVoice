#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UEGachaPool.h"
#include "UEGachaMachine.generated.h"

class UCameraComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UUEFieldServerBridgeComponent;

// Waiting 을 뒤에 붙였다. 블루프린트가 이 열거의 **값**으로 비교하므로
// 중간에 끼우면 BP_GachaMachine 의 기존 분기가 조용히 다른 상태를 가리킨다.
UENUM(BlueprintType)
enum class EUEGachaState : uint8 { Ready, Turning, Dispensing, Result, Waiting };
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

	/** 서버가 아직 결과를 안 줬다. 세 바퀴를 다 돌린 뒤 기다리는 중이다. */
	UFUNCTION(BlueprintPure, Category="Gacha")
	bool IsWaitingForServer() const { return State == EUEGachaState::Waiting; }

	/** 이미 갖고 있던 종족이 나왔다. 결과 화면이 이걸로 문구를 바꾼다. */
	UFUNCTION(BlueprintPure, Category="Gacha")
	bool IsDuplicateResult() const { return bDuplicateResult; }
	const FUEGachaEntry& GetResult() const { return SelectedEntry; }
	void TurnHandle(float ClockwiseDegrees);
	bool CanTurn() const { return State == EUEGachaState::Ready || State == EUEGachaState::Turning; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void UpdateStage();
	void SimulateCapsules(float Seconds);

	// 로컬 플레이어 폰에 붙은 필드 브릿지. 뽑기방 레벨처럼 서버에 안 붙은
	// 곳에서는 nullptr 이고, 그때는 뽑기를 시작하지 않고 안내만 띄운다.
	UUEFieldServerBridgeComponent* FindBridge() const;

	// 서버에 추첨을 요청한다. 못 보내면 false 이고 StatusMessage 에 사유가 남는다.
	bool RequestServerDraw();

	void BeginDispense();

	UFUNCTION()
	void HandleServerGachaResult(const FUEFieldGachaResult& Result);

	FUEGachaEntry SelectedEntry;

	// 서버 결과를 기다리는 중인가. 한 기계만 뽑는 중일 수 있으므로, 브로드캐스트를
	// 받는 다른 기계들은 이 값으로 자기 것이 아닌 결과를 흘려보낸다.
	bool bAwaitingResult = false;
	bool bHasResult = false;

	// 결과보다 세 바퀴가 먼저 끝났다. 결과가 오는 즉시 배출을 시작한다.
	bool bDispenseWhenReady = false;

	bool bSubscribed = false;

	// 결과 화면 문구를 고르는 데 쓴다. 이미 갖고 있던 종족이면 꽝이다.
	bool bDuplicateResult = false;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Capsules;
	TArray<FVector> Velocities;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> StageMaterials;
	float DispenseElapsed = 0;
	float Pulse = 0;
};
