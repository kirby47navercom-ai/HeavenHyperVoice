#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "UEPokemonAnimInstance.generated.h"

class AUEPokemonCharacter;

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

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Pokemon")
	TObjectPtr<AUEPokemonCharacter> OwnerPokemon = nullptr;

	// XY 평면 기준 이동 속도다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	float GroundSpeed = 0.0f;

	// 캐릭터 정면 기준 이동 방향 각도다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	float DirectionAngle = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State")
	bool bIsRunning = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State")
	bool bIsFalling = false;

	// 이 값 이상이면 달리기 상태로 본다. 종별 애님 BP에서 필요하면 조정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Movement", meta = (ClampMin = "0.0"))
	float RunSpeedThreshold = 360.0f;
};
