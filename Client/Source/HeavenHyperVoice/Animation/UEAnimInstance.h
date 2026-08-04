// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "UEAnimInstance.generated.h"

class AUEPlayerCharacter;

/**
 * Animation data bridge for the player character.
 *
 * Animation Blueprints should read these values instead of reaching into movement code directly.
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
};
