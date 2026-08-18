#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "../Data/UEHHVCustomizationTypes.h"
#include "UEHHVCustomizationWidget.generated.h"

class AUEHHVCustomizationPreviewActor;
class UTextBlock;
class USkeletalMesh;
class UTexture2D;
class UScrollBox;
class UUniformGridPanel;
class UUEHHVCustomizationWidget;
class UUEHHVCustomizationCatalog;
class UVerticalBox;
class UButton;

UCLASS()
class HEAVENHYPERVOICE_API UUEHHVOptionButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(
		UUEHHVCustomizationWidget* InOwner,
		EUEHHVCustomizationCategory InCategory,
		int32 InIndex);

private:
	UFUNCTION() void HandleClicked();

	UPROPERTY(Transient) TObjectPtr<UUEHHVCustomizationWidget> OwnerWidget = nullptr;
	EUEHHVCustomizationCategory Category = EUEHHVCustomizationCategory::Body;
	int32 Index = INDEX_NONE;
};

UCLASS()
class HEAVENHYPERVOICE_API UUEHHVColorButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(
		UUEHHVCustomizationWidget* InOwner,
		EUEHHVColorChannel InChannel,
		FLinearColor InColor);

	EUEHHVColorChannel GetChannel() const { return Channel; }
	FLinearColor GetColor() const { return Color; }

private:
	UFUNCTION() void HandleClicked();

	UPROPERTY(Transient) TObjectPtr<UUEHHVCustomizationWidget> OwnerWidget = nullptr;
	EUEHHVColorChannel Channel = EUEHHVColorChannel::Skin;
	FLinearColor Color = FLinearColor::White;
};

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUEHHVCustomizationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SetPreviewActor(AUEHHVCustomizationPreviewActor* InPreviewActor);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SetCatalog(UUEHHVCustomizationCatalog* InCatalog);

	UFUNCTION(BlueprintPure, Category = "Customization")
	AUEHHVCustomizationPreviewActor* GetPreviewActor() const { return PreviewActor; }

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SelectOption(EUEHHVCustomizationCategory Category, int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SelectGender(EUEHHVGender Gender);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SelectColor(EUEHHVColorChannel Channel, FLinearColor Color);

	void OpenCategory(EUEHHVCustomizationCategory Category);

	UFUNCTION(BlueprintPure, Category = "Customization")
	int32 GetOptionCount(EUEHHVCustomizationCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	FString GetOptionLabel(EUEHHVCustomizationCategory Category, int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	UTexture2D* GetOptionIcon(EUEHHVCustomizationCategory Category, int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	USkeletalMesh* GetOptionMesh(EUEHHVCustomizationCategory Category, int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	const FUEHHVAppearance& GetAppearance() const;

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void RefreshFromPreview();

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void StartWithCurrentAppearance();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<UUEHHVCustomizationCatalog> Catalog = nullptr;

private:
	bool BindDesignerInterface();
	void RebuildCategories();
	void RebuildOptions();
	void RebuildParameterControls();
	void SynchronizeControls();
	int32 GetVisibleOptionCount(EUEHHVCustomizationCategory Category) const;
	int32 GetActualOptionIndex(EUEHHVCustomizationCategory Category, int32 VisibleIndex) const;
	int32 GetSelectedIndex(EUEHHVCustomizationCategory Category) const;
	FLinearColor GetChannelColor(EUEHHVColorChannel Channel) const;
	TArray<FLinearColor> GetPaletteColors(EUEHHVColorChannel Channel) const;
	UTextBlock* CreateText(const FString& Text, int32 FontSize, const FLinearColor& Color);
	void AddSectionTitle(UVerticalBox* Parent, const FString& Text);
	void AddPaletteRow(UVerticalBox* Parent, const FString& Label, EUEHHVColorChannel Channel);
	UButton* FindDesignerStartButton() const;
	void BindStartButton();
	bool IsPointerOverPreviewArea(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) const;
	static FString GetCategoryLabel(EUEHHVCustomizationCategory Category);

	UFUNCTION()
	void HandleStartClicked();

	UPROPERTY(BlueprintReadOnly, Category = "Customization", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AUEHHVCustomizationPreviewActor> PreviewActor = nullptr;

	UPROPERTY(Transient) TObjectPtr<UVerticalBox> CategoryList = nullptr;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ColorControls = nullptr;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> OptionScroll = nullptr;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> OptionGrid = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OptionTitle = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OptionCount = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText = nullptr;
	UPROPERTY(Transient) TObjectPtr<UButton> StartButton = nullptr;
	UPROPERTY(Transient) TArray<TObjectPtr<UUEHHVColorButton>> ColorButtons;

	FUEHHVAppearance CachedAppearance;
	EUEHHVCustomizationCategory CurrentCategory = EUEHHVCustomizationCategory::Body;
	bool bSynchronizingControls = false;
	bool bRotatingPreview = false;
	bool bPanningPreview = false;
	FVector2D LastPointerScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Customization|Travel")
	FName StartLevelName = TEXT("PlayerTestLevel");
};
