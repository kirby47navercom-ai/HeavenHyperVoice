#include "UEHHVCustomizationWidget.h"

#include "../Preview/UEHHVCustomizationPreviewActor.h"
#include "../../../Pokemon/UEPokemonSpeciesData.h"
#include "../../../System/UEGameInstance.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Engine/Texture2D.h"

namespace
{
	void AddUniquePaletteColor(TArray<FLinearColor>& Colors, const FLinearColor& Color)
	{
		const FLinearColor ClampedColor = Color.GetClamped();
		for (const FLinearColor& ExistingColor : Colors)
		{
			if (ExistingColor.Equals(ClampedColor, 0.015f))
			{
				return;
			}
		}
		Colors.Add(ClampedColor);
	}
}

void UUECharacterCreationEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SelectButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UUECharacterCreationEntryWidget::NativeDestruct()
{
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
	}
	Super::NativeDestruct();
}

void UUECharacterCreationEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	EntryData = Cast<UUECharacterCreationEntryData>(ListItemObject);
	if (!EntryData)
	{
		return;
	}

	if (LabelText)
	{
		LabelText->SetText(EntryData->Label);
	}
	if (IconImage)
	{
		IconImage->SetBrushFromTexture(EntryData->Icon);
		IconImage->SetVisibility(EntryData->Icon ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ColorSwatch)
	{
		ColorSwatch->SetBrushColor(EntryData->Color);
		ColorSwatch->SetVisibility(
			EntryData->Kind == EUECharacterCreationEntryKind::Color
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (SelectedMarker)
	{
		SelectedMarker->SetVisibility(
			EntryData->bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UUECharacterCreationEntryWidget::HandleClicked()
{
	if (!EntryData)
	{
		return;
	}
	if (EntryData->CustomizationOwner)
	{
		EntryData->CustomizationOwner->HandleEntryActivated(EntryData);
	}
	else if (EntryData->StarterOwner)
	{
		EntryData->StarterOwner->HandleEntryActivated(EntryData);
	}
}

void UUEHHVCustomizationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindDesignerEvents();

	RefreshFromPreview();
	RefreshCategoryLabels();
	OpenCategory(CurrentCategory);
	OpenColorChannel(CurrentColorChannel);
}

void UUEHHVCustomizationWidget::NativeDestruct()
{
	UnbindDesignerEvents();
	Super::NativeDestruct();
}

void UUEHHVCustomizationWidget::SetPreviewActor(AUEHHVCustomizationPreviewActor* InPreviewActor)
{
	PreviewActor = InPreviewActor;
	if (PreviewActor)
	{
		FUEHHVAppearance PendingAppearance;
		if (const UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
			GameInstance && GameInstance->GetPendingHHVAppearance(PendingAppearance))
		{
			PreviewActor->ApplyAppearance(PendingAppearance);
		}
	}
	if (PreviewActor && !Catalog)
	{
		Catalog = PreviewActor->GetCatalog();
	}
	RefreshFromPreview();
	RefreshCategoryLabels();
	RebuildAppearanceOptions();
	RebuildColorOptions();
}

void UUEHHVCustomizationWidget::SetCatalog(UUEHHVCustomizationCatalog* InCatalog)
{
	Catalog = InCatalog;
	RefreshCategoryLabels();
	RebuildAppearanceOptions();
	RebuildColorOptions();
}

void UUEHHVCustomizationWidget::SelectOption(EUEHHVCustomizationCategory Category, int32 Index)
{
	if (PreviewActor)
	{
		PreviewActor->SelectOption(Category, Index);
		RefreshFromPreview();
	}
	RebuildAppearanceOptions();
}

void UUEHHVCustomizationWidget::SelectGender(EUEHHVGender Gender)
{
	if (PreviewActor)
	{
		PreviewActor->SelectGender(Gender);
		RefreshFromPreview();
	}
	RebuildAppearanceOptions();
}

void UUEHHVCustomizationWidget::SelectColor(EUEHHVColorChannel Channel, FLinearColor Color)
{
	if (PreviewActor)
	{
		PreviewActor->SetColor(Channel, Color);
		RefreshFromPreview();
	}
	RebuildColorOptions();
}

void UUEHHVCustomizationWidget::OpenCategory(EUEHHVCustomizationCategory Category)
{
	CurrentCategory = Category;
	if (OptionTitleText)
	{
		// 선택 영역에는 현재 카테고리 이름만 간결하게 표시한다.
		OptionTitleText->SetText(GetCategoryTitle(Category));
	}
	RebuildAppearanceOptions();
}

int32 UUEHHVCustomizationWidget::GetOptionCount(EUEHHVCustomizationCategory Category) const
{
	return Catalog ? Catalog->GetOptionCount(Category) : 0;
}

FString UUEHHVCustomizationWidget::GetOptionLabel(EUEHHVCustomizationCategory Category, int32 Index) const
{
	if (!Catalog)
	{
		return FString();
	}

	const FUEHHVCustomizationOption& Option = Catalog->GetOption(Category, Index);
	if (Category == EUEHHVCustomizationCategory::Body)
	{
		if (Option.Id.Equals(TEXT("TypeA"), ESearchCase::IgnoreCase))
		{
			return TEXT("체형 1");
		}
		if (Option.Id.Equals(TEXT("TypeB"), ESearchCase::IgnoreCase))
		{
			return TEXT("체형 2");
		}
	}
	return Option.DisplayName.IsEmpty() ? Option.Id : Option.DisplayName;
}

UTexture2D* UUEHHVCustomizationWidget::GetOptionIcon(EUEHHVCustomizationCategory Category, int32 Index) const
{
	return Catalog ? Catalog->GetOption(Category, Index).Icon : nullptr;
}

const FUEHHVAppearance& UUEHHVCustomizationWidget::GetAppearance() const
{
	return PreviewActor ? PreviewActor->GetAppearance() : CachedAppearance;
}

void UUEHHVCustomizationWidget::RefreshFromPreview()
{
	if (PreviewActor)
	{
		CachedAppearance = PreviewActor->GetAppearance();
	}
}

void UUEHHVCustomizationWidget::HandleEntryActivated(UUECharacterCreationEntryData* EntryData)
{
	if (!EntryData)
	{
		return;
	}

	switch (EntryData->Kind)
	{
	case EUECharacterCreationEntryKind::Appearance:
		SelectOption(EntryData->AppearanceCategory, EntryData->AppearanceIndex);
		break;
	case EUECharacterCreationEntryKind::Color:
		SelectColor(EntryData->ColorChannel, EntryData->Color);
		break;
	default:
		break;
	}
}

void UUEHHVCustomizationWidget::RebuildAppearanceOptions()
{
	if (!AppearanceOptionList || !Catalog)
	{
		return;
	}

	AppearanceEntryItems.Reset();
	TArray<UObject*> ListItems;
	const int32 FirstIndex = GetFirstVisibleOptionIndex(CurrentCategory);
	const int32 Count = GetVisibleOptionCount(CurrentCategory);
	const int32 SelectedIndex = GetSelectedIndex(CurrentCategory);
	for (int32 Offset = 0; Offset < Count; ++Offset)
	{
		const int32 ActualIndex = FirstIndex + Offset;
		UUECharacterCreationEntryData* Entry = NewObject<UUECharacterCreationEntryData>(this);
		Entry->Kind = EUECharacterCreationEntryKind::Appearance;
		Entry->CustomizationOwner = this;
		Entry->AppearanceCategory = CurrentCategory;
		Entry->AppearanceIndex = ActualIndex;
		// 외형 선택은 썸네일 자체로 구분하므로 카드 안에 번호와 이름을 중복 표시하지 않는다.
		Entry->Label = FText::GetEmpty();
		Entry->Icon = GetOptionIcon(CurrentCategory, ActualIndex);
		Entry->bSelected = ActualIndex == SelectedIndex;
		AppearanceEntryItems.Add(Entry);
		ListItems.Add(Entry);
	}
	AppearanceOptionList->SetListItems(ListItems);

	// 선택 상태는 카드 테두리로만 보여 주며 하단 요약 문구는 사용하지 않는다.
}

void UUEHHVCustomizationWidget::RebuildColorOptions()
{
	if (!ColorOptionList)
	{
		return;
	}

	ColorEntryItems.Reset();
	TArray<UObject*> ListItems;
	const TArray<FLinearColor> Colors = BuildPaletteColors(CurrentColorChannel);
	if (!Colors.IsEmpty())
	{
		const FLinearColor SelectedColor = GetChannelColor(CurrentColorChannel);
		for (const FLinearColor& Color : Colors)
		{
			UUECharacterCreationEntryData* Entry = NewObject<UUECharacterCreationEntryData>(this);
			Entry->Kind = EUECharacterCreationEntryKind::Color;
			Entry->CustomizationOwner = this;
			Entry->ColorChannel = CurrentColorChannel;
			Entry->Color = Color.GetClamped();
			Entry->bSelected = SelectedColor.Equals(Entry->Color, KINDA_SMALL_NUMBER);
			ColorEntryItems.Add(Entry);
			ListItems.Add(Entry);
		}
	}
	ColorOptionList->SetListItems(ListItems);
}

void UUEHHVCustomizationWidget::RefreshCategoryLabels()
{
	SetCategoryLabel(BodyCategoryButton_Label, EUEHHVCustomizationCategory::Body);
	SetCategoryLabel(HeadCategoryButton_Label, EUEHHVCustomizationCategory::Head);
	SetCategoryLabel(HairCategoryButton_Label, EUEHHVCustomizationCategory::Hair);
	SetCategoryLabel(EyeCategoryButton_Label, EUEHHVCustomizationCategory::Eyes);
	SetCategoryLabel(OutfitCategoryButton_Label, EUEHHVCustomizationCategory::BodyEquipment);
}

void UUEHHVCustomizationWidget::SetCategoryLabel(
	UTextBlock* Label,
	EUEHHVCustomizationCategory Category)
{
	if (!Label)
	{
		return;
	}

	// 카테고리 버튼에는 이름만 보여 주고 항목 개수는 표시하지 않는다.
	Label->SetText(GetCategoryTitle(Category));
}

void UUEHHVCustomizationWidget::OpenColorChannel(EUEHHVColorChannel Channel)
{
	CurrentColorChannel = Channel;
	RebuildColorOptions();
}

int32 UUEHHVCustomizationWidget::GetSelectedIndex(EUEHHVCustomizationCategory Category) const
{
	const FUEHHVAppearance& Appearance = GetAppearance();
	switch (Category)
	{
	case EUEHHVCustomizationCategory::Body: return Appearance.BodyIndex;
	case EUEHHVCustomizationCategory::Head: return Appearance.HeadIndex;
	case EUEHHVCustomizationCategory::Hair: return Appearance.HairIndex;
	case EUEHHVCustomizationCategory::Eyes: return Appearance.EyeIndex;
	case EUEHHVCustomizationCategory::BodyEquipment: return Appearance.BodyEquipmentIndex;
	default: return INDEX_NONE;
	}
}

int32 UUEHHVCustomizationWidget::GetFirstVisibleOptionIndex(EUEHHVCustomizationCategory Category) const
{
	if (Category == EUEHHVCustomizationCategory::Body)
	{
		return FirstBodyOptionIndex;
	}
	if (Category == EUEHHVCustomizationCategory::BodyEquipment)
	{
		return FirstOutfitOptionIndex;
	}
	return 0;
}

int32 UUEHHVCustomizationWidget::GetVisibleOptionCount(EUEHHVCustomizationCategory Category) const
{
	const int32 Available = FMath::Max(0, GetOptionCount(Category) - GetFirstVisibleOptionIndex(Category));
	if (Category == EUEHHVCustomizationCategory::Body)
	{
		return FMath::Min(Available, MaxBodyOptions);
	}
	if (Category == EUEHHVCustomizationCategory::BodyEquipment)
	{
		return FMath::Min(Available, MaxOutfitOptions);
	}
	return Available;
}

FLinearColor UUEHHVCustomizationWidget::GetChannelColor(EUEHHVColorChannel Channel) const
{
	const FUEHHVAppearance& Appearance = GetAppearance();
	switch (Channel)
	{
	case EUEHHVColorChannel::Skin: return Appearance.SkinColor;
	case EUEHHVColorChannel::Hair: return Appearance.HairColor;
	case EUEHHVColorChannel::Eye: return Appearance.EyeColor;
	default: return FLinearColor::Transparent;
	}
}

const TArray<FLinearColor>* UUEHHVCustomizationWidget::GetCatalogColors(EUEHHVColorChannel Channel) const
{
	if (!Catalog)
	{
		return nullptr;
	}
	switch (Channel)
	{
	case EUEHHVColorChannel::Skin: return &Catalog->SkinColors;
	case EUEHHVColorChannel::Hair: return &Catalog->HairColors;
	case EUEHHVColorChannel::Eye: return &Catalog->EyeColors;
	default: return nullptr;
	}
}

TArray<FLinearColor> UUEHHVCustomizationWidget::BuildPaletteColors(EUEHHVColorChannel Channel) const
{
	TArray<FLinearColor> Colors;
	if (const TArray<FLinearColor>* CatalogColors = GetCatalogColors(Channel))
	{
		for (const FLinearColor& Color : *CatalogColors)
		{
			AddUniquePaletteColor(Colors, Color);
		}
	}

	// 원본 커마 화면의 촘촘한 색상표를 복원하되, WBP에는 색상 데이터만 전달한다.
	if (Channel == EUEHHVColorChannel::Skin)
	{
		for (const float Value : {1.0f, 0.92f, 0.84f, 0.76f, 0.66f, 0.56f})
		{
			for (const float Saturation : {0.12f, 0.22f, 0.34f, 0.46f, 0.58f, 0.70f})
			{
				AddUniquePaletteColor(Colors, FLinearColor::MakeFromHSV8(
					22,
					static_cast<uint8>(Saturation * 255.0f),
					static_cast<uint8>(Value * 255.0f)));
			}
		}
	}
	else if (Channel == EUEHHVColorChannel::Hair)
	{
		for (const float Value : {0.18f, 0.32f, 0.48f, 0.68f, 0.86f})
		{
			for (const uint8 Hue : {0, 14, 24, 34, 48, 64, 96, 128, 160, 190, 216, 238})
			{
				AddUniquePaletteColor(Colors, FLinearColor::MakeFromHSV8(
					Hue,
					185,
					static_cast<uint8>(Value * 255.0f)));
			}
		}
	}
	else
	{
		for (const float Value : {0.30f, 0.46f, 0.62f, 0.78f})
		{
			for (const uint8 Hue : {0, 24, 42, 74, 105, 135, 160, 186, 210, 232})
			{
				AddUniquePaletteColor(Colors, FLinearColor::MakeFromHSV8(
					Hue,
					180,
					static_cast<uint8>(Value * 255.0f)));
			}
		}
		AddUniquePaletteColor(Colors, FLinearColor(0.08f, 0.08f, 0.08f));
		AddUniquePaletteColor(Colors, FLinearColor(0.72f, 0.72f, 0.72f));
	}

	// 팔레트 위치를 매번 예측할 수 있도록 RGB 16진수 코드(#000000 -> #FFFFFF) 순서로 정렬한다.
	Colors.Sort([](const FLinearColor& Left, const FLinearColor& Right)
	{
		const auto ToRgbCode = [](const FLinearColor& Color)
		{
			const FColor Quantized = Color.GetClamped().ToFColorSRGB();
			return (static_cast<uint32>(Quantized.R) << 16)
				| (static_cast<uint32>(Quantized.G) << 8)
				| static_cast<uint32>(Quantized.B);
		};

		return ToRgbCode(Left) < ToRgbCode(Right);
	});

	return Colors;
}

FText UUEHHVCustomizationWidget::GetCategoryTitle(EUEHHVCustomizationCategory Category) const
{
	switch (Category)
	{
	case EUEHHVCustomizationCategory::Body: return BodyCategoryTitle;
	case EUEHHVCustomizationCategory::Head: return HeadCategoryTitle;
	case EUEHHVCustomizationCategory::Hair: return HairCategoryTitle;
	case EUEHHVCustomizationCategory::Eyes: return EyeCategoryTitle;
	case EUEHHVCustomizationCategory::BodyEquipment: return OutfitCategoryTitle;
	default: return FText::GetEmpty();
	}
}

void UUEHHVCustomizationWidget::BindDesignerEvents()
{
	BodyCategoryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBodyCategoryClicked);
	HeadCategoryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleHeadCategoryClicked);
	HairCategoryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleHairCategoryClicked);
	EyeCategoryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEyeCategoryClicked);
	OutfitCategoryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOutfitCategoryClicked);
	TypeAButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTypeAClicked);
	TypeBButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTypeBClicked);
	SkinColorButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSkinColorClicked);
	HairColorButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleHairColorClicked);
	EyeColorButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEyeColorClicked);
	CompleteButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCompleteClicked);
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);
}

void UUEHHVCustomizationWidget::UnbindDesignerEvents()
{
	if (BodyCategoryButton) BodyCategoryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleBodyCategoryClicked);
	if (HeadCategoryButton) HeadCategoryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleHeadCategoryClicked);
	if (HairCategoryButton) HairCategoryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleHairCategoryClicked);
	if (EyeCategoryButton) EyeCategoryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleEyeCategoryClicked);
	if (OutfitCategoryButton) OutfitCategoryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleOutfitCategoryClicked);
	if (TypeAButton) TypeAButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleTypeAClicked);
	if (TypeBButton) TypeBButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleTypeBClicked);
	if (SkinColorButton) SkinColorButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleSkinColorClicked);
	if (HairColorButton) HairColorButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleHairColorClicked);
	if (EyeColorButton) EyeColorButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleEyeColorClicked);
	if (CompleteButton) CompleteButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCompleteClicked);
	if (BackButton) BackButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleBackClicked);
}

void UUEHHVCustomizationWidget::HandleBodyCategoryClicked() { OpenCategory(EUEHHVCustomizationCategory::Body); }
void UUEHHVCustomizationWidget::HandleHeadCategoryClicked() { OpenCategory(EUEHHVCustomizationCategory::Head); }
void UUEHHVCustomizationWidget::HandleHairCategoryClicked() { OpenCategory(EUEHHVCustomizationCategory::Hair); }
void UUEHHVCustomizationWidget::HandleEyeCategoryClicked() { OpenCategory(EUEHHVCustomizationCategory::Eyes); }
void UUEHHVCustomizationWidget::HandleOutfitCategoryClicked() { OpenCategory(EUEHHVCustomizationCategory::BodyEquipment); }
void UUEHHVCustomizationWidget::HandleTypeAClicked() { SelectGender(EUEHHVGender::TypeA); }
void UUEHHVCustomizationWidget::HandleTypeBClicked() { SelectGender(EUEHHVGender::TypeB); }
void UUEHHVCustomizationWidget::HandleSkinColorClicked() { OpenColorChannel(EUEHHVColorChannel::Skin); }
void UUEHHVCustomizationWidget::HandleHairColorClicked() { OpenColorChannel(EUEHHVColorChannel::Hair); }
void UUEHHVCustomizationWidget::HandleEyeColorClicked() { OpenColorChannel(EUEHHVColorChannel::Eye); }
void UUEHHVCustomizationWidget::HandleCompleteClicked()
{
	RefreshFromPreview();
	if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		GameInstance->SetPendingHHVAppearance(CachedAppearance);
	}
	OnCustomizationConfirmed.Broadcast();
}

void UUEHHVCustomizationWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}

void UUEStarterPokemonWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);
	SetStatusMessage(ReadyMessage);
	RebuildOptions();
}

void UUEStarterPokemonWidget::NativeDestruct()
{
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleBackClicked);
	}
	Super::NativeDestruct();
}

void UUEStarterPokemonWidget::HandleEntryActivated(UUECharacterCreationEntryData* EntryData)
{
	if (!EntryData || EntryData->Kind != EUECharacterCreationEntryKind::StarterPokemon)
	{
		return;
	}
	SelectedStarterPokemon = EntryData->StarterPokemon;
	RebuildOptions();
}

void UUEStarterPokemonWidget::SetStatusMessage(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

void UUEStarterPokemonWidget::RebuildOptions()
{
	EntryItems.Reset();
	TArray<UObject*> ListItems;
	for (UUEPokemonSpeciesData* Pokemon : StarterPokemonOptions)
	{
		if (!Pokemon)
		{
			continue;
		}

		// 도감번호로 거른다. 배열 위치로 거르면 카탈로그에 종족을 끼워 넣는
		// 순간 다른 포켓몬이 스타터가 된다.
		if (!AllowedStarterDexNumbers.IsEmpty()
			&& !AllowedStarterDexNumbers.Contains(Pokemon->DexNumber))
		{
			continue;
		}

		UUECharacterCreationEntryData* Entry = NewObject<UUECharacterCreationEntryData>(this);
		Entry->Kind = EUECharacterCreationEntryKind::StarterPokemon;
		Entry->StarterOwner = this;
		Entry->StarterPokemon = Pokemon;
		FString PokemonDisplayName = Pokemon->GetName();
		PokemonDisplayName.RemoveFromStart(TEXT("DA_"));
		Entry->Label = FText::FromString(PokemonDisplayName);
		// 초상화는 WBP 기본값에서만 연결하며 에셋 경로를 코드에 고정하지 않는다.
		if (const TObjectPtr<UTexture2D>* Portrait = StarterPokemonPortraits.Find(Pokemon))
		{
			Entry->Icon = Portrait->Get();
		}
		Entry->bSelected = Pokemon == SelectedStarterPokemon;
		EntryItems.Add(Entry);
		ListItems.Add(Entry);
	}
	StarterPokemonList->SetListItems(ListItems);
	if (SelectedStarterPokemon)
	{
		FString PokemonDisplayName = SelectedStarterPokemon->GetName();
		PokemonDisplayName.RemoveFromStart(TEXT("DA_"));
		SelectedPartnerText->SetText(FText::FromString(PokemonDisplayName));
	}
	else
	{
		SelectedPartnerText->SetText(FText::GetEmpty());
	}
}

void UUEStarterPokemonWidget::HandleConfirmClicked()
{
	if (!SelectedStarterPokemon)
	{
		SetStatusMessage(SelectionRequiredMessage);
		return;
	}
	OnStarterConfirmed.Broadcast(SelectedStarterPokemon);
}

void UUEStarterPokemonWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}
