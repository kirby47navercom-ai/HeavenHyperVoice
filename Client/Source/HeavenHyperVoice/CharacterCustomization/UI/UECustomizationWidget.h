#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UECustomizationWidget.generated.h"

class AUECustomizationPreviewActor;
class UHorizontalBox;
class USlider;
class UTextBlock;
class UVerticalBox;

/** Runtime UMG surface for the standalone customization scene. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUECustomizationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SetPreviewActor(AUECustomizationPreviewActor* InPreviewActor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildInterface();
	void SynchronizeControls();
	void ApplyCurrentAppearance();
	void RefreshValueLabels();
	UTextBlock* CreateText(const FString& Text, int32 FontSize, const FLinearColor& Color);
	class UButton* CreateTextButton(const FString& Text);
	void AddSectionTitle(UVerticalBox* Parent, const FString& Text);
	void AddCycleRow(UVerticalBox* Parent, const FString& Label, class UButton*& OutPrevious, UTextBlock*& OutValue, class UButton*& OutNext);
	void AddSliderRow(UVerticalBox* Parent, const FString& Label, USlider*& OutSlider);
	void AddColorRow(UVerticalBox* Parent, const FString& Label, const TArray<FLinearColor>& Colors, TArray<class UButton*>& OutButtons);

	UFUNCTION() void PreviousBody();
	UFUNCTION() void NextBody();
	UFUNCTION() void PreviousHair();
	UFUNCTION() void NextHair();
	UFUNCTION() void PreviousAccessory();
	UFUNCTION() void NextAccessory();
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
	UFUNCTION() void HairBlack();
	UFUNCTION() void HairBlue();
	UFUNCTION() void HairCoral();
	UFUNCTION() void OutfitCyan();
	UFUNCTION() void OutfitYellow();
	UFUNCTION() void OutfitRed();

	UPROPERTY(Transient)
	TObjectPtr<AUECustomizationPreviewActor> PreviewActor = nullptr;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> BodyValue = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HairValue = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AccessoryValue = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText = nullptr;
	UPROPERTY(Transient) TObjectPtr<USlider> HeightSlider = nullptr;
	UPROPERTY(Transient) TObjectPtr<USlider> HeadSizeSlider = nullptr;
	UPROPERTY(Transient) TObjectPtr<USlider> ShoulderSlider = nullptr;

	bool bSynchronizingControls = false;
};
