// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "UEAnimInstance.generated.h"

class AUEPlayerCharacter;

/**
 * 플레이어 애니메이션 블루프린트가 읽는 공통 부모 클래스다.
 * 이동 코드에 직접 접근하지 말고 아래 값만 보고 상태를 판단하게 둔다.
 */
UCLASS()
class HEAVENHYPERVOICE_API UUEAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Character")
	TObjectPtr<AUEPlayerCharacter> OwnerCharacter = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	float DirectionAngle = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	FVector2D MovementInput = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State")
	bool bIsRunning = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State")
	bool bIsRolling = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State")
	bool bIsFalling = false;

	// 커마 체형 기준 성별 상태다. 현재 커마 코드에서 TypeA는 여성, TypeB는 남성이다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Character")
	bool bIsFemale = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Character")
	bool bIsMale = false;
};
