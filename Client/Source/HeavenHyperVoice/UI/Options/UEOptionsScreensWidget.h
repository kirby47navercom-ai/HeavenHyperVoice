#pragma once

#include "UEOptionsMenuWidget.h"
#include "Components/ComboBoxString.h"
#include "UEOptionsScreensWidget.generated.h"

class UComboBoxString;
class UCheckBox;
class UWidgetSwitcher;

/** 디자이너가 만든 개별 옵션 화면의 공통 뒤로 가기 동작을 담당한다. */
UCLASS(Abstract, Blueprintable)
class HEAVENHYPERVOICE_API UUEOptionsScreenWidget : public UUEWindowWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable, Category = "Options")
	FUEOptionsActionRequested OnScreenActionRequested;
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;
private:
	UFUNCTION()
	void GoBack();
};

/** 설정 컨트롤과 배치는 WBP_GameSettings에서 편집한다. */
UCLASS(Abstract, Blueprintable)
class HEAVENHYPERVOICE_API UUEGameSettingsWidget : public UUEOptionsScreenWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> QualityCombo;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> FrameLimitCombo;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> VSyncCheckBox;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ApplySettingsButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SettingsStatusText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> SettingsPages;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DisplayTab;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> GraphicsTab;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CategoryTitleText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> ShadowCombo;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> TextureCombo;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> EffectsCombo;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> ViewDistanceCombo;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> AntiAliasingCombo;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> FoliageCombo;
private:
	void LoadSettings();
	void SelectCategory(int32 Index);
	bool bUpdatingQuality = false;
	UFUNCTION()
	void ShowDisplay();
	UFUNCTION()
	void ShowGraphics();
	UFUNCTION()
	void OnPresetChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void OnDetailChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void ApplySettings();
};

/** 세션 명령은 WBP_OptionsConfirm에서 사용자가 확인한 뒤에만 실행한다. */
UCLASS(Abstract, Blueprintable)
class HEAVENHYPERVOICE_API UUEOptionsConfirmWidget : public UUEOptionsScreenWidget
{
	GENERATED_BODY()
public:
	void SetAction(FName ActionId);
protected:
	virtual void NativeConstruct() override;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ConfirmTitleText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ConfirmMessageText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;
private:
	FName PendingAction;
	UFUNCTION()
	void Confirm();
};
