// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UEPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UUEPlayerMovementSyncComponent;

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AUEPlayerCharacter();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsRunning() const { return bIsRunning; }

	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsRolling() const { return bIsRolling; }

	UFUNCTION(BlueprintPure, Category = "Character|State")
	FVector2D GetMovementInput() const { return MovementInput; }

	UFUNCTION(BlueprintPure, Category = "Character|State")
	FVector GetDesiredMovementDirection() const;

	UFUNCTION(BlueprintPure, Category = "Character|Movement Sync")
	UUEPlayerMovementSyncComponent* GetMovementSyncComponent() const { return MovementSyncComponent; }

	void SetMovementInput(const FVector2D& NewMovementInput);
	void ApplyServerMovementCorrection(const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bUseHardCorrection);

protected:
	virtual void BeginPlay() override;

	void ApplyLocalMovementInput();
	FVector GetCameraForwardAxis(const FRotator& ViewRotation) const;
	FVector GetCameraRightAxis(const FRotator& ViewRotation) const;
	FVector GetMoveDirectionFromInput(const FVector2D& Input, const FRotator& ViewRotation) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<UCameraComponent> FollowCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Movement Sync")
	TObjectPtr<UUEPlayerMovementSyncComponent> MovementSyncComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 260.0f;

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	FVector2D MovementInput = FVector2D::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	bool bIsRunning = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	bool bIsRolling = false;
};
