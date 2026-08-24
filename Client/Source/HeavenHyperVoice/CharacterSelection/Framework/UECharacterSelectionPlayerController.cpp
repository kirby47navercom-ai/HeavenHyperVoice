#include "UECharacterSelectionPlayerController.h"

#include "../UI/UECharacterSelectionWidget.h"

void AUECharacterSelectionPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!SelectionWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("캐릭터 선택 위젯 클래스가 블루프린트 기본값에 지정되지 않았습니다."));
		return;
	}

	SelectionWidget = CreateWidget<UUECharacterSelectionWidget>(this, SelectionWidgetClass);
	if (!SelectionWidget)
	{
		return;
	}

	SelectionWidget->AddToViewport(WidgetZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}
