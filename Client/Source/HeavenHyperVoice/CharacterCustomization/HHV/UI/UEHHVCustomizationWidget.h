#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "../Data/UEHHVCustomizationTypes.h"
#include "UEHHVCustomizationWidget.generated.h"

class AUEHHVCustomizationPreviewActor;
class UBorder;
class UButton;
class UEditableTextBox;
class UImage;
class UTextBlock;
class UTexture2D;
class UTileView;
class UUEHHVCustomizationWidget;
class UUEStarterPokemonWidget;
class UUEPokemonSpeciesData;
class UWidget;
class UWorld;

/** Blueprint 리스트 항목이 수행할 선택 종류다. */
UENUM(BlueprintType)
enum class EUECharacterCreationEntryKind : uint8
{
	Appearance,
	Color,
	StarterPokemon
};

/** 리스트가 WBP 항목에 전달하는 표시·선택 데이터다. UI 자체는 만들지 않는다. */
UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUECharacterCreationEntryData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Character Creation")
	EUECharacterCreationEntryKind Kind = EUECharacterCreationEntryKind::Appearance;

	UPROPERTY(BlueprintReadOnly, Category = "Character Creation")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Character Creation")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Character Creation")
	FLinearColor Color = FLinearColor::Transparent;

	UPROPERTY(BlueprintReadOnly, Category = "Character Creation")
	bool bSelected = false;

	UPROPERTY(Transient)
	TObjectPtr<UUEHHVCustomizationWidget> CustomizationOwner = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUEStarterPokemonWidget> StarterOwner = nullptr;

	EUEHHVCustomizationCategory AppearanceCategory = EUEHHVCustomizationCategory::Body;
	EUEHHVColorChannel ColorChannel = EUEHHVColorChannel::Skin;
	int32 AppearanceIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UUEPokemonSpeciesData> StarterPokemon = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUECustomizationConfirmedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUECustomizationBackRequestedSignature);

/** WBP가 배치한 버튼과 표시 요소에 리스트 데이터만 반영하는 항목 기반 위젯이다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUECharacterCreationEntryWidget
	: public UUserWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SelectButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ColorSwatch = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedMarker = nullptr;

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<UUECharacterCreationEntryData> EntryData = nullptr;
};

/** 캐릭터 커마와 스타팅 포켓몬 선택을 WBP 디자이너 화면에 연결한다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUEHHVCustomizationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Customization|Event")
	FUECustomizationConfirmedSignature OnCustomizationConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Customization|Event")
	FUECustomizationBackRequestedSignature OnBackRequested;

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

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void OpenCategory(EUEHHVCustomizationCategory Category);

	UFUNCTION(BlueprintPure, Category = "Customization")
	int32 GetOptionCount(EUEHHVCustomizationCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	FString GetOptionLabel(EUEHHVCustomizationCategory Category, int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	UTexture2D* GetOptionIcon(EUEHHVCustomizationCategory Category, int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	const FUEHHVAppearance& GetAppearance() const;

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void RefreshFromPreview();

	void HandleEntryActivated(UUECharacterCreationEntryData* EntryData);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<UUEHHVCustomizationCatalog> Catalog = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Text") FText BodyCategoryTitle;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Text") FText HeadCategoryTitle;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Text") FText HairCategoryTitle;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Text") FText EyeCategoryTitle;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Text") FText OutfitCategoryTitle;

	// 표시 범위도 WBP에서 조정해 카탈로그 교체 시 C++ 수정이 필요 없게 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Options", meta = (ClampMin = "0"))
	int32 FirstBodyOptionIndex = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Options", meta = (ClampMin = "0"))
	int32 MaxBodyOptions = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Options", meta = (ClampMin = "0"))
	int32 FirstOutfitOptionIndex = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Options", meta = (ClampMin = "0"))
	int32 MaxOutfitOptions = 14;

	// 프리뷰 조작 범위와 감도도 WBP 클래스 기본값에서 편집한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Preview Input") FKey RotatePreviewButton = EKeys::LeftMouseButton;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Preview Input") FKey PrimaryPanPreviewButton = EKeys::RightMouseButton;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Preview Input") FKey SecondaryPanPreviewButton = EKeys::MiddleMouseButton;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Preview Input", meta = (ClampMin = "0.0")) float PreviewYawSensitivity = 0.28f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Preview Input", meta = (ClampMin = "0.0")) float PreviewZoomSensitivity = 0.12f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Preview Input", meta = (ClampMin = "0.0", ClampMax = "1.0")) float PreviewAreaLeft = 0.16f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Preview Input", meta = (ClampMin = "0.0", ClampMax = "1.0")) float PreviewAreaRight = 0.86f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Preview Input", meta = (ClampMin = "0.0", ClampMax = "1.0")) float PreviewAreaTop = 0.02f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Preview Input", meta = (ClampMin = "0.0", ClampMax = "1.0")) float PreviewAreaBottom = 0.98f;

	// 아래 위젯의 배치·스타일·문구는 WBP_HHVCustomization이 소유한다.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> BodyCategoryButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> HeadCategoryButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> HairCategoryButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> EyeCategoryButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> OutfitCategoryButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> TypeAButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> TypeBButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SkinColorButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> HairColorButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> EyeColorButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CompleteButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> BackButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTileView> AppearanceOptionList = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTileView> ColorOptionList = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> OptionTitleText = nullptr;

	// 카테고리 이름은 WBP에 배치한 텍스트에 데이터만 전달한다.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BodyCategoryButton_Label = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HeadCategoryButton_Label = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HairCategoryButton_Label = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EyeCategoryButton_Label = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> OutfitCategoryButton_Label = nullptr;
	// 현재 선택한 항목 요약은 WBP의 원본 위치에 데이터만 갱신한다.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SelectionSummaryText = nullptr;

private:
	void RebuildAppearanceOptions();
	void RebuildColorOptions();
	void RefreshCategoryLabels();
	void SetCategoryLabel(UTextBlock* Label, EUEHHVCustomizationCategory Category);
	void OpenColorChannel(EUEHHVColorChannel Channel);
	int32 GetSelectedIndex(EUEHHVCustomizationCategory Category) const;
	int32 GetFirstVisibleOptionIndex(EUEHHVCustomizationCategory Category) const;
	int32 GetVisibleOptionCount(EUEHHVCustomizationCategory Category) const;
	FLinearColor GetChannelColor(EUEHHVColorChannel Channel) const;
	const TArray<FLinearColor>* GetCatalogColors(EUEHHVColorChannel Channel) const;
	TArray<FLinearColor> BuildPaletteColors(EUEHHVColorChannel Channel) const;
	FText GetCategoryTitle(EUEHHVCustomizationCategory Category) const;
	bool IsPointerOverPreviewArea(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) const;
	void BindDesignerEvents();
	void UnbindDesignerEvents();

	UFUNCTION() void HandleBodyCategoryClicked();
	UFUNCTION() void HandleHeadCategoryClicked();
	UFUNCTION() void HandleHairCategoryClicked();
	UFUNCTION() void HandleEyeCategoryClicked();
	UFUNCTION() void HandleOutfitCategoryClicked();
	UFUNCTION() void HandleTypeAClicked();
	UFUNCTION() void HandleTypeBClicked();
	UFUNCTION() void HandleSkinColorClicked();
	UFUNCTION() void HandleHairColorClicked();
	UFUNCTION() void HandleEyeColorClicked();
	UFUNCTION() void HandleCompleteClicked();
	UFUNCTION() void HandleBackClicked();

	UPROPERTY(BlueprintReadOnly, Category = "Customization", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AUEHHVCustomizationPreviewActor> PreviewActor = nullptr;

	UPROPERTY(Transient) TArray<TObjectPtr<UUECharacterCreationEntryData>> AppearanceEntryItems;
	UPROPERTY(Transient) TArray<TObjectPtr<UUECharacterCreationEntryData>> ColorEntryItems;

	FUEHHVAppearance CachedAppearance;
	EUEHHVCustomizationCategory CurrentCategory = EUEHHVCustomizationCategory::Body;
	EUEHHVColorChannel CurrentColorChannel = EUEHHVColorChannel::Skin;
	bool bRotatingPreview = false;
	bool bPanningPreview = false;
	FVector2D LastPointerScreenPosition = FVector2D::ZeroVector;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FUEStarterPokemonConfirmedSignature,
	UUEPokemonSpeciesData*,
	StarterPokemon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUEStarterPokemonBackRequestedSignature);

/** 캐릭터 생성의 마지막 단계에서 스타팅 포켓몬만 선택하는 WBP 기반 위젯이다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUEStarterPokemonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Starter Pokemon|Event")
	FUEStarterPokemonConfirmedSignature OnStarterConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Starter Pokemon|Event")
	FUEStarterPokemonBackRequestedSignature OnBackRequested;

	void HandleEntryActivated(UUECharacterCreationEntryData* EntryData);

	UFUNCTION(BlueprintCallable, Category = "Starter Pokemon")
	void SetStatusMessage(const FText& Message);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// WBP_StarterPokemon 기본값에 담긴 후보다. 이 목록에 무엇을 넣든 실제로
	// 표시되는 것은 AllowedStarterDexNumbers 에 있는 것뿐이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Pokemon")
	TArray<TObjectPtr<UUEPokemonSpeciesData>> StarterPokemonOptions;

	// 캐릭터를 만들 때 고를 수 있는 도감번호.
	//
	// 서버의 PokemonSpecies.h kStarterDex 와 같아야 한다. 권위는 서버에 있고
	// 이건 화면일 뿐이다 — 여기만 늘리면 서버가 거절하고, 서버만 늘리면 화면에
	// 안 뜬다. 둘 다 안전하게 실패한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Pokemon")
	TArray<int32> AllowedStarterDexNumbers = {1, 4, 7, 25, 133};

	// 포켓몬별 선택 초상화도 WBP 기본값에서 지정하며, 비워 두면 이름만 중앙에 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Pokemon")
	TMap<TObjectPtr<UUEPokemonSpeciesData>, TObjectPtr<UTexture2D>> StarterPokemonPortraits;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Pokemon|Text")
	FText ReadyMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Pokemon|Text")
	FText SelectionRequiredMessage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTileView> StarterPokemonList = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SelectedPartnerText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText = nullptr;

private:
	void RebuildOptions();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleBackClicked();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UUECharacterCreationEntryData>> EntryItems;

	UPROPERTY(Transient)
	TObjectPtr<UUEPokemonSpeciesData> SelectedStarterPokemon = nullptr;
};
