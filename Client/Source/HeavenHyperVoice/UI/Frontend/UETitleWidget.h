#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UETitleWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUETitleContinueRequestedSignature);

/** 타이틀 WBP의 입력만 전달하는 네이티브 기반 위젯이다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUETitleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Frontend|Event")
	FUETitleContinueRequestedSignature OnContinueRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 버튼의 모양과 위치는 WBP_Title에서만 편집한다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ContinueButton = nullptr;

private:
	UFUNCTION()
	void HandleContinueClicked();
};
