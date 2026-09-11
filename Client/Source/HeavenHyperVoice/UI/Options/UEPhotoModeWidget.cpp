#include "UEPhotoModeWidget.h"
#include "../../Player/UEPlayerController.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

void UUEPhotoModeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CaptureButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Capture);
	ExitButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Exit);
	ZoomInButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ZoomIn);
	ZoomOutButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ZoomOut);
	ZoomSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::ZoomChanged);
}

void UUEPhotoModeWidget::Capture()
{
	if (auto* PC = GetOwningPlayer<AUEPlayerController>()) PC->TakePhoto();
}

void UUEPhotoModeWidget::Exit()
{
	if (auto* PC = GetOwningPlayer<AUEPlayerController>()) PC->ExitPhotoMode();
}

void UUEPhotoModeWidget::ZoomIn()
{
	if (auto* PC = GetOwningPlayer<AUEPlayerController>()) PC->SetPhotoZoom(PC->GetPhotoZoom() + .1f);
}

void UUEPhotoModeWidget::ZoomOut()
{
	if (auto* PC = GetOwningPlayer<AUEPlayerController>()) PC->SetPhotoZoom(PC->GetPhotoZoom() - .1f);
}

void UUEPhotoModeWidget::ZoomChanged(float Value)
{
	if (auto* PC = GetOwningPlayer<AUEPlayerController>()) PC->SetPhotoZoom(Value);
}

void UUEPhotoModeWidget::UpdateZoom(float Value, float Magnification)
{
	ZoomSlider->SetValue(Value);
	ZoomText->SetText(FText::FromString(FString::Printf(TEXT("%.1f ×"), Magnification)));
}

void UUEPhotoModeWidget::SetStatus(const FText& Text) { StatusText->SetText(Text); }

FReply UUEPhotoModeWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		if (!Event.IsRepeat()) Exit();
		return FReply::Handled();
	}
	if (Event.GetKey() == EKeys::SpaceBar)
	{
		if (!Event.IsRepeat()) Capture();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UUEPhotoModeWidget::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (auto* PC = GetOwningPlayer<AUEPlayerController>())
		PC->SetPhotoZoom(PC->GetPhotoZoom() + Event.GetWheelDelta() * .08f);
	return FReply::Handled();
}
