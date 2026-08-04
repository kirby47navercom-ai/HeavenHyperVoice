// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPlayerController.h"

#include "../UI/Login/UELoginWidget.h"

#include "Blueprint/UserWidget.h"

AUEPlayerController::AUEPlayerController()
{
	LoginWidgetClass = UUELoginWidget::StaticClass();
	bShowMouseCursor = true;
}

void AUEPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (bShowLoginOnBeginPlay)
	{
		ShowLoginScreen();
	}
}

void AUEPlayerController::ShowLoginScreen()
{
	if (!LoginWidgetInstance)
	{
		TSubclassOf<UUELoginWidget> WidgetClassToCreate = LoginWidgetClass;
		if (!WidgetClassToCreate)
		{
			WidgetClassToCreate = UUELoginWidget::StaticClass();
		}

		LoginWidgetInstance = CreateWidget<UUELoginWidget>(this, WidgetClassToCreate);
		if (!LoginWidgetInstance)
		{
			return;
		}
	}

	LoginWidgetInstance->OnLoginSucceeded.AddUniqueDynamic(this, &ThisClass::HandleLoginSucceeded);
	LoginWidgetInstance->AddToViewport(LoginWidgetZOrder);

	// Login UI should receive keyboard and mouse input before gameplay starts.
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(LoginWidgetInstance->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AUEPlayerController::HideLoginScreen()
{
	if (LoginWidgetInstance)
	{
		LoginWidgetInstance->OnLoginSucceeded.RemoveDynamic(this, &ThisClass::HandleLoginSucceeded);
		LoginWidgetInstance->RemoveFromParent();
	}

	// After login, return control to the character movement input path.
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
}

void AUEPlayerController::HandleLoginSucceeded(const FString& UserId, const FString& Nickname)
{
	// Gameplay input is enabled only after the local account subsystem accepted both credentials.
	HideLoginScreen();
	BP_OnLocalLoginSucceeded(UserId, Nickname);
}
