// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "UEPlayerController.generated.h"

class AUEPlayerCharacter;
class UUELoginWidget;
class UUEDataAsset;

/**
 * PlayerController for the client entry flow.
 *
 * It shows the login widget first, then switches back to game input when
 * HideLoginScreen is called by Blueprint or future login code.
 */
UCLASS()
class HEAVENHYPERVOICE_API AUEPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AUEPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Login")
	void ShowLoginScreen();

	UFUNCTION(BlueprintCallable, Category = "Login")
	void HideLoginScreen();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

	// Blueprint can continue the client flow after C++ has accepted the credentials.
	UFUNCTION(BlueprintImplementableEvent, Category = "Login", meta = (DisplayName = "On Local Login Succeeded"))
	void BP_OnLocalLoginSucceeded(const FString& UserId, const FString& Nickname);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UUEDataAsset> InputData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Look", meta = (ClampMin = "0.0"))
	float LookYawRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Look", meta = (ClampMin = "0.0"))
	float LookPitchRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Login")
	TSubclassOf<UUELoginWidget> LoginWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login")
	bool bShowLoginOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login")
	int32 LoginWidgetZOrder = 100;

private:
	UFUNCTION()
	void HandleLoginSucceeded(const FString& UserId, const FString& Nickname);

	void AddDefaultMappingContext() const;
	void BindGameplayInput();
	void BindMoveInput(class UEnhancedInputComponent* EnhancedInputComponent);
	void BindLookInput(class UEnhancedInputComponent* EnhancedInputComponent);
	void BindActionInput(class UEnhancedInputComponent* EnhancedInputComponent);
	AUEPlayerCharacter* GetControlledPlayerCharacter() const;
	bool HasPendingPalworldAppearance() const;

	void HandleMove(const FInputActionValue& Value);
	void HandleMoveStopped(const FInputActionValue& Value);
	void HandleLookYaw(const FInputActionValue& Value);
	void HandleLookPitch(const FInputActionValue& Value);
	void HandleRunStarted(const FInputActionValue& Value);
	void HandleRunStopped(const FInputActionValue& Value);
	void HandlePokemonToggle(const FInputActionValue& Value);

	UPROPERTY(Transient)
	TObjectPtr<UUELoginWidget> LoginWidgetInstance = nullptr;
};
