#pragma once

#include "CoreMinimal.h"
#include "../UEWindowWidget.h"
#include "Blueprint/UserWidget.h"
#include "UEOptionsMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUEOptionsActionRequested, FName, ActionId);

/** 재사용 가능한 메뉴 카드다. 배치, 글꼴, 버튼 스타일은 WBP_OptionsCard에서 편집한다. */
UCLASS(Abstract, Blueprintable)
class HEAVENHYPERVOICE_API UUEOptionsCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options|Content")
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options|Content")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options|Content")
	TObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options|Motion", meta = (ClampMin = "0.0"))
	float HoverLift = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options|Motion", meta = (ClampMin = "0.0"))
	float ResponseSpeed = 14.0f;

	UPROPERTY(BlueprintAssignable, Category = "Options")
	FUEOptionsActionRequested OnActionRequested;

	UFUNCTION(BlueprintCallable, Category = "Options")
	void RefreshContent();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CardButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

private:
	UFUNCTION()
	void HandleClicked();

	float HoverAmount = 0.0f;
};

/** 디자이너가 편집하는 사이드 메뉴다. 세션 명령은 소유 컨트롤러가 처리한다. */
UCLASS(Abstract, Blueprintable)
class HEAVENHYPERVOICE_API UUEOptionsMenuWidget : public UUEWindowWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Options")
	FUEOptionsActionRequested OnMenuActionRequested;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options|Motion")
	bool bAnimateEntrance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options|Motion", meta = (ClampMin = "0.0"))
	float EntranceDuration = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options|Motion")
	float EntranceOffset = 720.0f;

	/** 기존 메뉴 인스턴스를 다시 표시할 때 등장 연출을 처음부터 재생한다. */
	UFUNCTION(BlueprintCallable, Category = "Options")
	void PlayEntrance();

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Options")
	FName LastRequestedAction;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> MenuSurface;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerNameText;

private:
	UFUNCTION()
	void HandleActionRequested(FName ActionId);

	UFUNCTION()
	void HandleCloseClicked();

	void ApplyEntrance(float Progress);
	float EntranceElapsed = 0.0f;
};

/** 왼쪽 위에 보이는 메뉴 버튼의 배치는 WBP_OptionsHUD에서 편집한다. */
UCLASS(Abstract, Blueprintable)
class HEAVENHYPERVOICE_API UUEOptionsHUDWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MenuButton;
private:
	UFUNCTION()
	void OpenMenu();
};
