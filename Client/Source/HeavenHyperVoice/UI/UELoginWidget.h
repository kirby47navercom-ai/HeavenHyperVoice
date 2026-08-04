// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UELoginWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UEditableTextBox;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUELoginSubmittedSignature, const FString&, UserId, const FString&, Password);

/**
 * 서버 코드를 건드리지 않고 클라이언트 첫 화면만 구성하는 로그인 위젯이다.
 *
 * BP로 자식 위젯을 만들면 TitleText, PanelWidth 같은 값을 Details에서 바꿀 수 있다.
 * 실제 서버 접속은 여기서 직접 하지 않고 OnLoginSubmitted 이벤트로 밖에 넘긴다.
 */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUELoginWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UUELoginWidget(const FObjectInitializer& ObjectInitializer);

	// 화면에 보이는 문구다. BP 자식에서 팀원이 바로 수정할 수 있게 열어둔다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText TitleText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText SubtitleText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText IdHintText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText PasswordHintText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText LoginButtonText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText ReadyStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText EmptyInputStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText SubmittedStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Style")
	FLinearColor InputTextColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.0f);

	// 화면 중앙 기준의 UI 크기다. BP에서 값만 바꿔도 배치가 같이 따라간다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout", meta = (ClampMin = "240.0"))
	float PanelWidth = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout", meta = (ClampMin = "32.0"))
	float InputHeight = 46.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout", meta = (ClampMin = "32.0"))
	float ButtonHeight = 52.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout")
	float TitleOffsetY = -220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout")
	float SubtitleOffsetY = -170.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout")
	float IdInputOffsetY = -75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout")
	float PasswordInputOffsetY = -18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout")
	float LoginButtonOffsetY = 52.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout")
	float StatusOffsetY = 125.0f;

	// 버튼이 눌렸을 때 호출된다. 실제 로그인 통신은 클라 전용 LoginClient나 BP에서 이어 붙이면 된다.
	UPROPERTY(BlueprintAssignable, Category = "Login|Event")
	FUELoginSubmittedSignature OnLoginSubmitted;

	UFUNCTION(BlueprintCallable, Category = "Login")
	FString GetUserId() const;

	UFUNCTION(BlueprintCallable, Category = "Login")
	FString GetPassword() const;

	UFUNCTION(BlueprintCallable, Category = "Login")
	void SetStatusMessage(const FText& Message);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildLoginLayout();
	UCanvasPanelSlot* AddCenteredCanvasSlot(UWidget* Widget, const FVector2D& Position, const FVector2D& Size, int32 ZOrder) const;

	UFUNCTION()
	void HandleLoginClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BackgroundBorder = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleBlock = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleBlock = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> IdInputBox = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> PasswordInputBox = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LoginButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LoginButtonLabel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusBlock = nullptr;
};
