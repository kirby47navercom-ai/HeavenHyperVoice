#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "../Pokemon/UEPokemonCharacter.h"
#include "UEPokemonAnimInstance.generated.h"

class UBlendSpace;
class UAnimMontage;
class UAnimSequence;

/**
 * 포켓몬 애니메이션 블루프린트가 읽는 공통 부모 클래스다.
 * 서버나 AI 이동 방식이 바뀌어도 블루프린트는 아래 값만 보면 된다.
 */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUEPokemonAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;

	// 서버가 선택한 필드 행동을 현재 종의 실제 애니메이션 시퀀스로 바꾸어 재생한다.
	// Start / Loop / End가 있는 행동은 등록된 시퀀스만 골라 순서대로 이어 붙인다.
	UFUNCTION(BlueprintCallable, Category = "Animation|Field")
	bool PlayFieldAnimation(EUEPokemonFieldAnimation FieldAnimation, int32 LoopCount = 1);

	UFUNCTION(BlueprintCallable, Category = "Animation|Field")
	void StopFieldAnimation(float BlendOutTime = 0.15f);

	UFUNCTION(BlueprintPure, Category = "Animation|Field")
	bool IsFieldAnimationPlaying() const { return bIsFieldAnimationPlaying; }

	// 서버가 승인한 공격 종류를 현재 포켓몬 종의 Attack 시퀀스로 바꾸어 재생한다.
	// RangeAttack02처럼 Start / Loop / End가 있는 공격도 하나의 명령으로 처리한다.
	UFUNCTION(BlueprintCallable, Category = "Animation|Combat")
	bool PlayAttackAnimation(EUEPokemonAttackAnimation AttackAnimation, int32 LoopCount = 1);

	UFUNCTION(BlueprintCallable, Category = "Animation|Combat")
	void StopAttackAnimation(float BlendOutTime = 0.1f);

	UFUNCTION(BlueprintPure, Category = "Animation|Combat")
	bool IsAttackAnimationPlaying() const { return bIsAttackAnimationPlaying; }

protected:
	void StopCurrentActionAnimation(float BlendOutTime);
	void QueueActionSequence(UAnimSequence* Sequence, int32 LoopCount = 1);
	void PlayNextActionSequence();
	void ScheduleNextActionSequence();
	void HandleActionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Pokemon")
	TObjectPtr<AUEPokemonCharacter> OwnerPokemon = nullptr;

	// XY 평면 기준 이동 속도다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	float GroundSpeed = 0.0f;

	// 언리얼 캐릭터의 정면은 로컬 +X다. +X 방향 이동은 양수, 뒤쪽 이동은 음수다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	float ForwardSpeed = 0.0f;

	// 캐릭터 정면 기준 이동 방향 각도다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	float DirectionAngle = 0.0f;

	// 종별 자식 AnimBP가 지정하는 Idle / Walk / Run 1D BlendSpace다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Locomotion")
	TObjectPtr<UBlendSpace> LocomotionBlendSpace = nullptr;

	// Run 기준 속도를 넘어갈 때 발 미끄러짐을 줄이기 위한 재생 배율이다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Locomotion")
	float LocomotionPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Locomotion", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float MaxLocomotionPlayRate = 1.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State")
	bool bIsRunning = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State")
	bool bIsFalling = false;

	// 부모 AnimBP의 같은 이름 Slot 노드를 통해 필드 행동과 공격을 Locomotion 위에 재생한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Field")
	FName FieldAnimationSlotName = TEXT("DefaultSlot");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Field", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FieldAnimationBlendInTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Field", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FieldAnimationBlendOutTime = 0.15f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Field")
	EUEPokemonFieldAnimation CurrentFieldAnimation = EUEPokemonFieldAnimation::None;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Field")
	bool bIsFieldAnimationPlaying = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Combat")
	EUEPokemonAttackAnimation CurrentAttackAnimation = EUEPokemonAttackAnimation::None;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Combat")
	bool bIsAttackAnimationPlaying = false;

	// 현재 종의 Walk / Run 기준 속도다. 자식 AnimBP의 Class Defaults에서 조정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Locomotion", meta = (ClampMin = "1.0"))
	float WalkAnimationSpeed = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Locomotion", meta = (ClampMin = "1.0"))
	float RunAnimationSpeed = 300.0f;

	// 필드 행동과 공격 시퀀스를 같은 방식으로 이어 재생하는 공통 대기열이다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> ActionAnimationQueue;

	TArray<int32> ActionAnimationLoopCounts;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveActionMontage = nullptr;

	// Slot 정리가 예상보다 늦을 때 현재 조각을 잃지 않고 다음 게임 프레임에 다시 시도한 횟수다.
	int32 ActionSequenceStartRetryCount = 0;
};
