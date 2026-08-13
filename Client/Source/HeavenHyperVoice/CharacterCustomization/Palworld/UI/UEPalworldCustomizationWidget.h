#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "../Data/UEPalworldCustomizationTypes.h"
#include "UEPalworldCustomizationWidget.generated.h"

class AUEPalworldCustomizationPreviewActor;
class UTextBlock;
class USkeletalMesh;
class UTexture2D;
class UScrollBox;
class UUniformGridPanel;
class UUEPalworldCustomizationWidget;
class UUEPalworldCustomizationCatalog;
class UVerticalBox;
class UButton;

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
class HEAVENHYPERVOICE_API UUEPalworldColorButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(
		UUEPalworldCustomizationWidget* InOwner,
		EUEPalworldColorChannel InChannel,
		FLinearColor InColor);

	EUEPalworldColorChannel GetChannel() const { return Channel; }
	FLinearColor GetColor() const { return Color; }

private:
	UFUNCTION() void HandleClicked();

	UPROPERTY(Transient) TObjectPtr<UUEPalworldCustomizationWidget> OwnerWidget = nullptr;
	EUEPalworldColorChannel Channel = EUEPalworldColorChannel::Skin;
	FLinearColor Color = FLinearColor::White;
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
	EUEPalworldScaleChannel Channel = EUEPalworldScaleChannel::TorsoSize;
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

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void StartWithCurrentAppearance();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld")
	TObjectPtr<UUEPalworldCustomizationCatalog> Catalog = nullptr;

private:
	bool BindDesignerInterface();
	void RebuildCategories();
	void RebuildOptions();
	void RebuildParameterControls();
	void SynchronizeControls();
	int32 GetVisibleOptionCount(EUEPalworldCustomizationCategory Category) const;
	int32 GetActualOptionIndex(EUEPalworldCustomizationCategory Category, int32 VisibleIndex) const;
	int32 GetSelectedIndex(EUEPalworldCustomizationCategory Category) const;
	FLinearColor GetChannelColor(EUEPalworldColorChannel Channel) const;
	TArray<FLinearColor> GetPaletteColors(EUEPalworldColorChannel Channel) const;
	float GetScaleValue(EUEPalworldScaleChannel Channel) const;
	UTextBlock* CreateText(const FString& Text, int32 FontSize, const FLinearColor& Color);
	void AddSectionTitle(UVerticalBox* Parent, const FString& Text);
	void AddPaletteRow(UVerticalBox* Parent, const FString& Label, EUEPalworldColorChannel Channel);
	void AddScaleRow(UVerticalBox* Parent, const FString& Label, EUEPalworldScaleChannel Channel);
	UButton* FindDesignerStartButton() const;
	void BindStartButton();
	bool IsPointerOverPreviewArea(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) const;
	static FString GetCategoryLabel(EUEPalworldCustomizationCategory Category);

	UFUNCTION()
	void HandleStartClicked();

	UPROPERTY(BlueprintReadOnly, Category = "Palworld", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AUEPalworldCustomizationPreviewActor> PreviewActor = nullptr;

	UPROPERTY(Transient) TObjectPtr<UVerticalBox> CategoryList = nullptr;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ColorControls = nullptr;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ScaleControls = nullptr;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> OptionScroll = nullptr;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> OptionGrid = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OptionTitle = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OptionCount = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText = nullptr;
	UPROPERTY(Transient) TObjectPtr<UButton> StartButton = nullptr;
	UPROPERTY(Transient) TArray<TObjectPtr<UUEPalworldColorButton>> ColorButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UUEPalworldScaleSlider>> ScaleSliders;

	FUEPalworldAppearance CachedAppearance;
	EUEPalworldCustomizationCategory CurrentCategory = EUEPalworldCustomizationCategory::Body;
	bool bSynchronizingControls = false;
	bool bRotatingPreview = false;
	bool bPanningPreview = false;
	FVector2D LastPointerScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Palworld|Travel")
	FName StartLevelName = TEXT("PlayerTestLevel");
};
