#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UETitleWidget.generated.h"

class UButton;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUETitleContinueRequestedSignature);

/** 타이틀 배경 그림을 어느 장면으로 보여 주는지. */
UENUM(BlueprintType)
enum class EUEFrontendShot : uint8
{
	// 배경 그림의 구름 쪽만 확대하고 로고와 시작 안내를 보여 준다.
	Title,
	// 전체 그림으로 빠진 장면. 서버 접속·로그인 패널이 이 위에 올라간다.
	Menu
};

/**
 * 타이틀 화면이자, 서버 접속·로그인 화면 뒤에 깔리는 배경 겹이다.
 *
 * 컨트롤러가 서버·로그인 화면으로 넘어가도 이 위젯은 지우지 않고 장면만 Menu 로 바꾼다.
 * 그래야 배경과 로고가 끊기지 않고 줌아웃된다. 움직임은 WBP_Title 의 애니메이션이 맡고,
 * 여기서는 이름으로 찾아 재생만 한다.
 */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUETitleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Frontend|Event")
	FUETitleContinueRequestedSignature OnContinueRequested;

	/** 장면을 바꾼다. bAnimate 가 거짓이거나 처음 정하는 장면이면 애니메이션 끝 모습으로 바로 맞춘다. */
	void SetShot(EUEFrontendShot NewShot, bool bAnimate);

	UFUNCTION(BlueprintPure, Category = "Frontend")
	EUEFrontendShot GetShot() const { return Shot; }

	/** 게임을 켜고 타이틀이 처음 뜰 때 로고 등장을 재생한다. */
	void PlayIntro();

	/** WBP 에 ZoomOut 애니메이션이 있는지. 없으면 줌을 기다릴 필요가 없다. */
	bool HasZoomAnimation() const { return ZoomOut != nullptr; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// Enter 입력만 쓰는 화면에서는 버튼이 없어도 된다.
	// 버튼을 추가할 경우 모양과 위치는 WBP_Title에서만 편집한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ContinueButton = nullptr;

	// 로고 조각이 하나씩 들어오고 시작 안내가 나타나는 애니메이션.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> LogoIntro = nullptr;

	// Title 장면(시작) → Menu 장면(끝). 배경 확대, 로고 위치, 시작 안내 숨김을 함께 담는다.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ZoomOut = nullptr;

	// 로고 둥실, 배경 숨쉬기, 빛줄기처럼 계속 도는 움직임. 위젯이 뜨는 동안 반복한다.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Idle = nullptr;

private:
	UFUNCTION()
	void HandleContinueClicked();

	void RequestContinue();

	// 길이 0 으로 재생해 끝(또는 시작) 모습으로 한 번 맞춘다.
	void SnapAnimation(UWidgetAnimation* Animation, bool bToEnd);

	EUEFrontendShot Shot = EUEFrontendShot::Title;
	bool bShotApplied = false;
};
