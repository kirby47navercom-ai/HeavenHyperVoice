#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "../Data/UECharacterCustomizationTypes.h"
#include "UECustomizationWidget.generated.h"

class AUECustomizationPreviewActor;
class UImage;
class USlider;
class UTextBlock;
class UTexture2D;
class UUniformGridPanel;
class UVerticalBox;
class UUECustomizationWidget;

/** Button with an explicit VRoid category or preset payload. */
UCLASS()
class HEAVENHYPERVOICE_API UUECustomizationOptionButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UUECustomizationWidget* InOwner, EUECustomizationPart InPart, int32 InIndex);

private:
	UFUNCTION() void HandleClicked();

	UPROPERTY(Transient) TObjectPtr<UUECustomizationWidget> OwnerWidget = nullptr;
	EUECustomizationPart Part = EUECustomizationPart::FaceSkin;
	int32 Index = INDEX_NONE;
};

/** Runtime VRoid-style character editor used by L_CharacterCustomization. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUECustomizationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SetPreviewActor(AUECustomizationPreviewActor* InPreviewActor);

	void OpenCategory(EUECustomizationPart Part);
	void SelectPartOption(EUECustomizationPart Part, int32 Index);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	enum class EEditorSection : uint8
	{
		Body,
		Face,
		Hairstyle,
		Outfit,
		Accessory
	};

	void BuildInterface();
	void SetSection(EEditorSection Section);
	void RebuildCategories();
	void RebuildCatalog();
	void SynchronizeControls();
	int32 GetSelectedIndex(EUECustomizationPart Part) const;
	UTextBlock* CreateText(const FString& Text, int32 FontSize, const FLinearColor& Color);
	UButton* CreateTextButton(const FString& Text);
	void AddSectionTitle(UVerticalBox* Parent, const FString& Text);
	void AddSliderRow(UVerticalBox* Parent, const FString& Label, TObjectPtr<USlider>& OutSlider);
	void AddColorRow(UVerticalBox* Parent, const FString& Label, const TArray<FLinearColor>& Colors, TArray<UButton*>& OutButtons);

	UFUNCTION() void ShowBody();
	UFUNCTION() void ShowFace();
	UFUNCTION() void ShowHairstyle();
	UFUNCTION() void ShowOutfit();
	UFUNCTION() void ShowAccessory();
	UFUNCTION() void HeightChanged(float Value);
	UFUNCTION() void HeadSizeChanged(float Value);
	UFUNCTION() void ShoulderWidthChanged(float Value);
	UFUNCTION() void RotateLeft();
	UFUNCTION() void RotateRight();
	UFUNCTION() void Save();
	UFUNCTION() void Reset();
	UFUNCTION() void Randomize();
	UFUNCTION() void SkinLight();
	UFUNCTION() void SkinMedium();
	UFUNCTION() void SkinDeep();
	UFUNCTION() void HairOriginal();
	UFUNCTION() void HairBlue();
	UFUNCTION() void HairCoral();
	UFUNCTION() void EyeOriginal();
	UFUNCTION() void EyeBlue();
	UFUNCTION() void EyeBrown();
	UFUNCTION() void LipNatural();
	UFUNCTION() void LipSoft();
	UFUNCTION() void LipDeep();
	UFUNCTION() void OutfitOriginal();
	UFUNCTION() void OutfitCyan();
	UFUNCTION() void OutfitRed();

	UPROPERTY(Transient) TObjectPtr<AUECustomizationPreviewActor> PreviewActor = nullptr;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> CategoryList = nullptr;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> OptionGrid = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CatalogTitle = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CatalogCount = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText = nullptr;
	UPROPERTY(Transient) TObjectPtr<USlider> HeightSlider = nullptr;
	UPROPERTY(Transient) TObjectPtr<USlider> HeadSizeSlider = nullptr;
	UPROPERTY(Transient) TObjectPtr<USlider> ShoulderSlider = nullptr;

	EEditorSection CurrentSection = EEditorSection::Face;
	EUECustomizationPart CurrentPart = EUECustomizationPart::EyeIris;
	bool bSynchronizingControls = false;
};
