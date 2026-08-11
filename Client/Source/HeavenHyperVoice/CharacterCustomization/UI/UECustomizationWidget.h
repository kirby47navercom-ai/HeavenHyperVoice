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
class USpinBox;
class UUECustomizationWidget;

enum class EUECustomizationColorChannel : uint8
{
	Skin,
	Hair,
	Eye,
	Lip,
	Outfit,
	Top,
	Bottom,
	Onepiece,
	Shoes,
	Accessory
};

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

/** Palette swatch that applies an sRGB color immediately. */
UCLASS()
class HEAVENHYPERVOICE_API UUECustomizationColorButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(
		UUECustomizationWidget* InOwner,
		EUECustomizationColorChannel InChannel,
		const FLinearColor& InColor);

private:
	UFUNCTION() void HandleClicked();

	UPROPERTY(Transient) TObjectPtr<UUECustomizationWidget> OwnerWidget = nullptr;
	EUECustomizationColorChannel Channel = EUECustomizationColorChannel::Skin;
	FLinearColor Color = FLinearColor::White;
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
	void SelectColor(EUECustomizationColorChannel Channel, const FLinearColor& Color);

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
	void AddColorRow(
		UVerticalBox* Parent,
		const FString& Label,
		EUECustomizationColorChannel Channel,
		const TArray<FLinearColor>& Colors,
		TArray<UButton*>& OutButtons);
	void AddRGBColorRow(
		UVerticalBox* Parent,
		const FString& Label,
		TObjectPtr<USpinBox>& OutRed,
		TObjectPtr<USpinBox>& OutGreen,
		TObjectPtr<USpinBox>& OutBlue);
	void UpdateColorChannel(EUECustomizationColorChannel Channel, int32 Component, float Value);
	void SynchronizeColorControls();
	static FColor ToSRGB8(const FLinearColor& Color);

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
	UFUNCTION() void SkinRedChanged(float Value);
	UFUNCTION() void SkinGreenChanged(float Value);
	UFUNCTION() void SkinBlueChanged(float Value);
	UFUNCTION() void HairRedChanged(float Value);
	UFUNCTION() void HairGreenChanged(float Value);
	UFUNCTION() void HairBlueChanged(float Value);
	UFUNCTION() void EyeRedChanged(float Value);
	UFUNCTION() void EyeGreenChanged(float Value);
	UFUNCTION() void EyeBlueChanged(float Value);
	UFUNCTION() void LipRedChanged(float Value);
	UFUNCTION() void LipGreenChanged(float Value);
	UFUNCTION() void LipBlueChanged(float Value);
	UFUNCTION() void OutfitRedChanged(float Value);
	UFUNCTION() void OutfitGreenChanged(float Value);
	UFUNCTION() void OutfitBlueChanged(float Value);
	UFUNCTION() void TopRedChanged(float Value);
	UFUNCTION() void TopGreenChanged(float Value);
	UFUNCTION() void TopBlueChanged(float Value);
	UFUNCTION() void BottomRedChanged(float Value);
	UFUNCTION() void BottomGreenChanged(float Value);
	UFUNCTION() void BottomBlueChanged(float Value);
	UFUNCTION() void OnepieceRedChanged(float Value);
	UFUNCTION() void OnepieceGreenChanged(float Value);
	UFUNCTION() void OnepieceBlueChanged(float Value);
	UFUNCTION() void ShoesRedChanged(float Value);
	UFUNCTION() void ShoesGreenChanged(float Value);
	UFUNCTION() void ShoesBlueChanged(float Value);
	UFUNCTION() void AccessoryRedChanged(float Value);
	UFUNCTION() void AccessoryGreenChanged(float Value);
	UFUNCTION() void AccessoryBlueChanged(float Value);

	UPROPERTY(Transient) TObjectPtr<AUECustomizationPreviewActor> PreviewActor = nullptr;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> CategoryList = nullptr;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> OptionGrid = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CatalogTitle = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CatalogCount = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText = nullptr;
	UPROPERTY(Transient) TObjectPtr<USlider> HeightSlider = nullptr;
	UPROPERTY(Transient) TObjectPtr<USlider> HeadSizeSlider = nullptr;
	UPROPERTY(Transient) TObjectPtr<USlider> ShoulderSlider = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> SkinRedInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> SkinGreenInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> SkinBlueInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> HairRedInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> HairGreenInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> HairBlueInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> EyeRedInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> EyeGreenInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> EyeBlueInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> LipRedInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> LipGreenInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> LipBlueInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> OutfitRedInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> OutfitGreenInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> OutfitBlueInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> TopRedInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> TopGreenInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> TopBlueInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> BottomRedInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> BottomGreenInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> BottomBlueInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> OnepieceRedInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> OnepieceGreenInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> OnepieceBlueInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> ShoesRedInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> ShoesGreenInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> ShoesBlueInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> AccessoryRedInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> AccessoryGreenInput = nullptr;
	UPROPERTY(Transient) TObjectPtr<USpinBox> AccessoryBlueInput = nullptr;

	EEditorSection CurrentSection = EEditorSection::Face;
	EUECustomizationPart CurrentPart = EUECustomizationPart::EyeIris;
	bool bSynchronizingControls = false;
};
