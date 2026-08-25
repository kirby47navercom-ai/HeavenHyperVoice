#include "UETitleWidget.h"

#include "Components/Button.h"
#include "InputCoreTypes.h"

void UUETitleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	ContinueButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleContinueClicked);
	SetKeyboardFocus();
}

void UUETitleWidget::NativeDestruct()
{
	if (ContinueButton)
	{
		ContinueButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleContinueClicked);
	}
	Super::NativeDestruct();
}

FReply UUETitleWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Enter
		|| InKeyEvent.GetKey() == EKeys::Virtual_Gamepad_Accept.GetVirtualKey())
	{
		OnContinueRequested.Broadcast();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UUETitleWidget::HandleContinueClicked()
{
	OnContinueRequested.Broadcast();
}
