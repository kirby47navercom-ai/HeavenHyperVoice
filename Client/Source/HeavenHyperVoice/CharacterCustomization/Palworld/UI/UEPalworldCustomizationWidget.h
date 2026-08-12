#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "../Data/UEPalworldCustomizationTypes.h"
#include "UEPalworldCustomizationWidget.generated.h"

class AUEPalworldCustomizationPreviewActor;
class UTextBlock;
class USkeletalMesh;
class UTexture2D;
class UUniformGridPanel;
class UUEPalworldCustomizationWidget;
class UUEPalworldCustomizationCatalog;
class UVerticalBox;

UCLASS()
class HEAVENHYPERVOICE_API UUEPalworldOptionButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(
		UUEPalworldCustomizationWidget* InOwner,
		EUEPalworldCustomizationCategory InCategory,
		int32 InIndex);

private:
	UFUNCTION() void HandleClicked();

	UPROPERTY(Transient) TObjectPtr<UUEPalworldCustomizationWidget> OwnerWidget = nullptr;
	EUEPalworldCustomizationCategory Category = EUEPalworldCustomizationCategory::Body;
	int32 Index = INDEX_NONE;
};

UCLASS()
class HEAVENHYPERVOICE_API UUEPalworldColorSpinBox : public USpinBox
{
	GENERATED_BODY()

public:
	void Configure(
		UUEPalworldCustomizationWidget* InOwner,
		EUEPalworldColorChannel InChannel,
		int32 InComponent);

	EUEPalworldColorChannel GetChannel() const { return Channel; }
	int32 GetComponent() const { return Component; }

private:
	UFUNCTION() void HandleValueChanged(float NewValue);

	UPROPERTY(Transient) TObjectPtr<UUEPalworldCustomizationWidget> OwnerWidget = nullptr;
	EUEPalworldColorChannel Channel = EUEPalworldColorChannel::Skin;
	int32 Component = 0;
};

UCLASS()
class HEAVENHYPERVOICE_API UUEPalworldScaleSlider : public USlider
{
	GENERATED_BODY()

public:
	void Configure(UUEPalworldCustomizationWidget* InOwner, EUEPalworldScaleChannel InChannel);
	EUEPalworldScaleChannel GetChannel() const { return Channel; }

private:
	UFUNCTION() void HandleValueChanged(float NewValue);

	UPROPERTY(Transient) TObjectPtr<UUEPalworldCustomizationWidget> OwnerWidget = nullptr;
	EUEPalworldScaleChannel Channel = EUEPalworldScaleChannel::Height;
};

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUEPalworldCustomizationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void SetPreviewActor(AUEPalworldCustomizationPreviewActor* InPreviewActor);

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void SetCatalog(UUEPalworldCustomizationCatalog* InCatalog);

	UFUNCTION(BlueprintPure, Category = "Palworld")
	AUEPalworldCustomizationPreviewActor* GetPreviewActor() const { return PreviewActor; }

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void SelectOption(EUEPalworldCustomizationCategory Category, int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void SelectGender(EUEPalworldGender Gender);

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void SelectColor(EUEPalworldColorChannel Channel, FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void SetScaleValue(EUEPalworldScaleChannel Channel, float Value);

	void OpenCategory(EUEPalworldCustomizationCategory Category);
	void SetColorComponent(EUEPalworldColorChannel Channel, int32 Component, float Value);
	void SetScaleFromSlider(EUEPalworldScaleChannel Channel, float Value);

	UFUNCTION(BlueprintPure, Category = "Palworld")
	int32 GetOptionCount(EUEPalworldCustomizationCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Palworld")
	FString GetOptionLabel(EUEPalworldCustomizationCategory Category, int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Palworld")
	UTexture2D* GetOptionIcon(EUEPalworldCustomizationCategory Category, int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Palworld")
	USkeletalMesh* GetOptionMesh(EUEPalworldCustomizationCategory Category, int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Palworld")
	const FUEPalworldAppearance& GetAppearance() const;

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void RefreshFromPreview();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld")
	TObjectPtr<UUEPalworldCustomizationCatalog> Catalog = nullptr;

private:
	void BuildInterface();
	void RebuildCategories();
	void RebuildOptions();
	void SynchronizeControls();
	int32 GetVisibleOptionCount(EUEPalworldCustomizationCategory Category) const;
	int32 GetActualOptionIndex(EUEPalworldCustomizationCategory Category, int32 VisibleIndex) const;
	int32 GetSelectedIndex(EUEPalworldCustomizationCategory Category) const;
	FLinearColor GetChannelColor(EUEPalworldColorChannel Channel) const;
	float GetScaleValue(EUEPalworldScaleChannel Channel) const;
	UTextBlock* CreateText(const FString& Text, int32 FontSize, const FLinearColor& Color);
	UButton* CreateTextButton(const FString& Text);
	void AddSectionTitle(UVerticalBox* Parent, const FString& Text);
	void AddInfoLine(UVerticalBox* Parent, const FString& Text);
	void AddRGBRow(UVerticalBox* Parent, const FString& Label, EUEPalworldColorChannel Channel);
	void AddScaleRow(UVerticalBox* Parent, const FString& Label, EUEPalworldScaleChannel Channel);
	static FString GetCategoryLabel(EUEPalworldCustomizationCategory Category);

	UPROPERTY(BlueprintReadOnly, Category = "Palworld", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AUEPalworldCustomizationPreviewActor> PreviewActor = nullptr;

	UPROPERTY(Transient) TObjectPtr<UVerticalBox> CategoryList = nullptr;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> OptionGrid = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OptionTitle = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OptionCount = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText = nullptr;
	UPROPERTY(Transient) TArray<TObjectPtr<UUEPalworldColorSpinBox>> ColorInputs;
	UPROPERTY(Transient) TArray<TObjectPtr<UUEPalworldScaleSlider>> ScaleSliders;

	FUEPalworldAppearance CachedAppearance;
	EUEPalworldCustomizationCategory CurrentCategory = EUEPalworldCustomizationCategory::Body;
	bool bSynchronizingControls = false;
};
