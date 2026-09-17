#include "UETitleWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"

void UUETitleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleContinueClicked);
	}
	if (Idle)
	{
		// 반복 횟수 0 은 끝없이 반복이다.
		PlayAnimation(Idle, 0.0f, 0);
	}
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
		RequestContinue();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UUETitleWidget::HandleContinueClicked()
{
	RequestContinue();
}

void UUETitleWidget::RequestContinue()
{
	// 서버·로그인 화면 뒤에 배경으로 깔려 있을 때는 시작 입력을 받지 않는다.
	if (Shot != EUEFrontendShot::Title)
	{
		return;
	}

	// 등장 중 첫 입력은 등장을 끝까지 건너뛰기만 한다. 다음 입력부터 넘어간다.
	if (LogoIntro && IsAnimationPlaying(LogoIntro))
	{
		SnapAnimation(LogoIntro, true);
		return;
	}

	OnContinueRequested.Broadcast();
}

void UUETitleWidget::SetShot(EUEFrontendShot NewShot, bool bAnimate)
{
	if (bShotApplied && Shot == NewShot)
	{
		return;
	}

	const bool bAnimateChange = bAnimate && bShotApplied;
	Shot = NewShot;
	bShotApplied = true;

	// Menu 장면에서는 서버·로그인 패널 뒤 배경일 뿐이다. 보이지 않는 시작 버튼이나 이 위젯이
	// 클릭·Tab 이동 포커스를 받으면 화면 가운데 아래에 포커스 사각형이 뜬다.
	const bool bTitleShot = Shot == EUEFrontendShot::Title;
	SetIsFocusable(bTitleShot);
	if (ContinueButton)
	{
		ContinueButton->SetVisibility(bTitleShot ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Shot == EUEFrontendShot::Menu && LogoIntro && IsAnimationPlaying(LogoIntro))
	{
		// 등장이 덜 끝난 채 줌아웃하면 로고 조각이 빠진 채 옮겨진다.
		SnapAnimation(LogoIntro, true);
	}

	if (!ZoomOut)
	{
		return;
	}

	const bool bToMenu = Shot == EUEFrontendShot::Menu;
	if (!bAnimateChange)
	{
		SnapAnimation(ZoomOut, bToMenu);
		return;
	}

	// 반대 방향으로 재생 중이면 그 자리에서 방향만 바뀐다.
	if (bToMenu)
	{
		PlayAnimationForward(ZoomOut);
	}
	else
	{
		PlayAnimationReverse(ZoomOut);
	}
}

void UUETitleWidget::PlayIntro()
{
	if (LogoIntro)
	{
		PlayAnimation(LogoIntro);
	}
}

void UUETitleWidget::SnapAnimation(UWidgetAnimation* Animation, bool bToEnd)
{
	// 끝 시각에서 시작하면 앞으로 재생은 끝 모습에서, 거꾸로 재생은 시작 모습에서 바로 멈춘다.
	PlayAnimation(
		Animation,
		Animation->GetEndTime(),
		1,
		bToEnd ? EUMGSequencePlayMode::Forward : EUMGSequencePlayMode::Reverse);
}
