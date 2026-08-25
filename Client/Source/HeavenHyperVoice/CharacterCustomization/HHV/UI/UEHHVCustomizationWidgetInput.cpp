#include "UEHHVCustomizationWidget.h"

#include "../Preview/UEHHVCustomizationPreviewActor.h"
#include "InputCoreTypes.h"
FReply UUEHHVCustomizationWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!PreviewActor || !IsPointerOverPreviewArea(InGeometry, InMouseEvent))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == RotatePreviewButton)
	{
		bRotatingPreview = true;
		bPanningPreview = false;
		LastPointerScreenPosition = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	if (InMouseEvent.GetEffectingButton() == PrimaryPanPreviewButton ||
		InMouseEvent.GetEffectingButton() == SecondaryPanPreviewButton)
	{
		bRotatingPreview = false;
		bPanningPreview = true;
		LastPointerScreenPosition = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UUEHHVCustomizationWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bRotatingPreview || bPanningPreview)
	{
		bRotatingPreview = false;
		bPanningPreview = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UUEHHVCustomizationWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!PreviewActor || (!bRotatingPreview && !bPanningPreview))
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const FVector2D CurrentPosition = InMouseEvent.GetScreenSpacePosition();
	const FVector2D Delta = CurrentPosition - LastPointerScreenPosition;
	LastPointerScreenPosition = CurrentPosition;

	if (bRotatingPreview)
	{
		PreviewActor->AddPreviewYaw(Delta.X * PreviewYawSensitivity);
	}
	else if (bPanningPreview)
	{
		PreviewActor->AddPreviewPan(Delta);
	}

	return FReply::Handled();
}

FReply UUEHHVCustomizationWidget::NativeOnMouseWheel(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!PreviewActor || !IsPointerOverPreviewArea(InGeometry, InMouseEvent))
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}

	PreviewActor->AddPreviewZoom(-InMouseEvent.GetWheelDelta() * PreviewZoomSensitivity);
	return FReply::Handled();
}

bool UUEHHVCustomizationWidget::IsPointerOverPreviewArea(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent) const
{
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D LocalSize = InGeometry.GetLocalSize();
	if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
	{
		return false;
	}

	// 좌우 UI 패널을 제외한 중앙 프리뷰 영역에서 회전/이동/확대 입력을 받는다.
	return LocalPosition.X > LocalSize.X * PreviewAreaLeft &&
		LocalPosition.X < LocalSize.X * PreviewAreaRight &&
		LocalPosition.Y > LocalSize.Y * PreviewAreaTop &&
		LocalPosition.Y < LocalSize.Y * PreviewAreaBottom;
}


