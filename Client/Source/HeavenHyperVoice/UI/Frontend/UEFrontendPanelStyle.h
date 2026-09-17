#pragma once

#include "CoreMinimal.h"
#include "UEFrontendPanelStyle.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;
class UUserWidget;
class UWidget;

// 서버 접속·로그인 패널이 함께 쓰는 모양 도우미: 상태 문구 스타일과 키보드·패드 포커스 링.

/**
 * 프론트엔드 화면 상태 문구의 종류.
 *
 * 문구는 WBP 기본값이나 서버 응답에서 오지만, 색·아이콘·연결 중 파형은 이 값을 보고
 * 고른다. OnStatusChanged 이벤트로도 넘어간다.
 */
UENUM(BlueprintType)
enum class EUEFrontendStatusKind : uint8
{
	Info,
	Pending,
	Error,
	Success
};

/** 상태 종류 하나의 모양. WBP 클래스 기본값의 StatusStyles 에 종류별로 넣는다. */
USTRUCT(BlueprintType)
struct HEAVENHYPERVOICE_API FUEFrontendStatusStyle
{
	GENERATED_BODY()

	// 문구와 아이콘에 같이 입히는 색.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	FLinearColor Color = FLinearColor::White;

	// 문구 앞 아이콘. 비우면 아이콘 자리를 비운다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	TObjectPtr<UTexture2D> Icon = nullptr;

	// 아이콘 대신 StatusWave(연결 중 파형)를 보여 줄지.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	bool bShowWave = false;
};

namespace UEFrontendStatus
{
	/**
	 * Styles 에 Kind 가 있으면 문구 색, 아이콘, 파형 표시를 맞춘다. 없으면 아무것도 바꾸지 않는다
	 * (WBP 가 OnStatusChanged 에서 직접 처리하는 경우).
	 */
	HEAVENHYPERVOICE_API void Apply(
		const TMap<EUEFrontendStatusKind, FUEFrontendStatusStyle>& Styles,
		EUEFrontendStatusKind Kind,
		UTextBlock* Text,
		UImage* Icon,
		UWidget* Wave);
}

/**
 * 키보드·패드 포커스 표시.
 *
 * WBP 에서 이름이 "<대상 위젯 이름>FocusRing" 인 위젯을 찾아, 대상이 키보드나 패드 이동으로
 * 포커스를 받았을 때만 보이게 한다. 마우스로 눌러 포커스가 간 경우에는 보이지 않는다
 * (Slate 가 이동으로 온 포커스만 “보여 줄 포커스”로 친다).
 */
struct HEAVENHYPERVOICE_API FUEFrontendFocusRings
{
	/** NativeConstruct 에서 한 번 부른다. */
	void Collect(UUserWidget* Owner);

	/** NativeTick 에서 부른다. 바뀐 링만 가시성을 바꾼다. */
	void Refresh();

private:
	struct FRing
	{
		TWeakObjectPtr<UWidget> Target;
		TWeakObjectPtr<UWidget> Ring;
	};

	TArray<FRing> Rings;
};
