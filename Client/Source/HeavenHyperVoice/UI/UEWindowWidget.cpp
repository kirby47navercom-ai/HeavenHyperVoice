#include "UEWindowWidget.h"

#include "../Player/UEPlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UUEWindowWidget::RemoveFromParent()
{
	Super::RemoveFromParent();
	if (auto* Controller = GetOwningPlayer<AUEPlayerController>())
		Controller->DeactivateUIWindow(this);
}

void UUEWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	// AddToViewport는 NativeConstruct가 끝난 뒤 뷰포트 슬롯을 설치하므로 다음 틱에 활성화한다.
	if (bActivateOnOpen && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]
		{
			if (IsInViewport() && IsVisible())
			{
				if (auto* Controller = GetOwningPlayer<AUEPlayerController>())
					Controller->ActivateUIWindow(this);
			}
		}));
	}
}

void UUEWindowWidget::NativeDestruct()
{
	if (auto* Controller = GetOwningPlayer<AUEPlayerController>())
		Controller->DeactivateUIWindow(this);
	Super::NativeDestruct();
}

FReply UUEWindowWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (auto* Controller = GetOwningPlayer<AUEPlayerController>())
		Controller->ActivateUIWindow(this);
	return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}

void UUEWindowWidget::NativeOnAddedToFocusPath(const FFocusEvent& Event)
{
	Super::NativeOnAddedToFocusPath(Event);
	if (auto* Controller = GetOwningPlayer<AUEPlayerController>())
		Controller->ActivateUIWindow(this, false);
}

FReply UUEWindowWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FReply Reply = Super::NativeOnMouseButtonDown(Geometry, Event);
	if (Reply.IsEventHandled()) return Reply;
	// 창 내부의 빈 영역도 UI 클릭으로 소비한다. 전체 화면 배치 컨테이너는
	// SelfHitTestInvisible이므로 창 밖의 월드는 계속 클릭을 받을 수 있다.
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton || Event.GetEffectingButton() == EKeys::RightMouseButton)
		return FReply::Handled();
	return Reply;
}

FReply UUEWindowWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (auto* Controller = GetOwningPlayer<AUEPlayerController>())
		if (Controller->HandleChatWindowKey(this, Event)) return FReply::Handled();
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
