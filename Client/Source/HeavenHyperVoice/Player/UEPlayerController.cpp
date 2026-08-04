// Fill out your copyright notice in the Description page of Project Settings.


#include "UEPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "../UI/UELoginWidget.h"

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
	if (LoginWidgetInstance)
	{
		LoginWidgetInstance->AddToViewport(LoginWidgetZOrder);
		return;
	}

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

	LoginWidgetInstance->AddToViewport(LoginWidgetZOrder);

	// 로그인 화면에서는 키보드/마우스 입력을 UI가 먼저 받게 한다.
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
		LoginWidgetInstance->RemoveFromParent();
	}

	// 로그인 이후 캐릭터 조작 화면으로 넘어갈 때 다시 게임 입력을 받게 한다.
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
}
