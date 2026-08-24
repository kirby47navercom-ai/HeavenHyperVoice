// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "UEPlayerController.generated.h"

class AUEPlayerCharacter;
class UUEDataAsset;

/** 실제 플레이 레벨의 이동과 액션 입력을 처리한다. */
UCLASS()
class HEAVENHYPERVOICE_API AUEPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AUEPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UUEDataAsset> InputData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Look", meta = (ClampMin = "0.0"))
	float LookYawRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Look", meta = (ClampMin = "0.0"))
	float LookPitchRate = 1.0f;

private:
	void AddDefaultMappingContext() const;
	void BindGameplayInput();
	void BindMoveInput(class UEnhancedInputComponent* EnhancedInputComponent);
	void BindLookInput(class UEnhancedInputComponent* EnhancedInputComponent);
	void BindActionInput(class UEnhancedInputComponent* EnhancedInputComponent);
	void BindRunInput(class UEnhancedInputComponent* EnhancedInputComponent);
	void BindJumpInput(UEnhancedInputComponent* EnhancedInputComponent);
	void BindRollInput(UEnhancedInputComponent* EnhancedInputComponent);
	void BindPokemonAttackInput(UEnhancedInputComponent* EnhancedInputComponent);
	AUEPlayerCharacter* GetControlledPlayerCharacter() const;
	bool HasPendingHHVAppearance() const;
	void PushMovementInputToCharacter();

	void HandleMove(const FInputActionValue& Value);
	void HandleMoveStopped(const FInputActionValue& Value);
	void HandleMoveForward(const FInputActionValue& Value);
	void HandleMoveForwardStopped(const FInputActionValue& Value);
	void HandleMoveBackward(const FInputActionValue& Value);
	void HandleMoveBackwardStopped(const FInputActionValue& Value);
	void HandleMoveRight(const FInputActionValue& Value);
	void HandleMoveRightStopped(const FInputActionValue& Value);
	void HandleMoveLeft(const FInputActionValue& Value);
	void HandleMoveLeftStopped(const FInputActionValue& Value);
	void HandleLookYaw(const FInputActionValue& Value);
	void HandleLookPitch(const FInputActionValue& Value);
	void HandleRunStarted(const FInputActionValue& Value);
	void HandleRunStopped(const FInputActionValue& Value);
	void HandleJump(const FInputActionValue& Value);
	void HandleRoll(const FInputActionValue& Value);
	void HandlePokemonToggle(const FInputActionValue& Value);
	void HandlePokemonAttack1(const FInputActionValue& Value);
	void HandlePokemonAttack2(const FInputActionValue& Value);
	void HandlePokemonAttack3(const FInputActionValue& Value);
	void HandlePokemonAttack4(const FInputActionValue& Value);
	void HandlePokemonAttackSlot(int32 AttackSlot);

	FVector2D PendingMovementInput = FVector2D::ZeroVector;
	float MaxWalkSpeed = 260.0f;
	float RunCross = 1.5f;
};
