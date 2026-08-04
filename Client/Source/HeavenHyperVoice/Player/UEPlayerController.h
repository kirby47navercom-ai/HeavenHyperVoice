// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UEPlayerController.generated.h"

class UUELoginWidget;

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

	// Blueprint can continue the client flow after C++ has accepted the credentials.
	UFUNCTION(BlueprintImplementableEvent, Category = "Login", meta = (DisplayName = "On Local Login Succeeded"))
	void BP_OnLocalLoginSucceeded(const FString& UserId, const FString& Nickname);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Login")
	TSubclassOf<UUELoginWidget> LoginWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login")
	bool bShowLoginOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login")
	int32 LoginWidgetZOrder = 100;

private:
	UFUNCTION()
	void HandleLoginSucceeded(const FString& UserId, const FString& Nickname);

	UPROPERTY(Transient)
	TObjectPtr<UUELoginWidget> LoginWidgetInstance = nullptr;
};
