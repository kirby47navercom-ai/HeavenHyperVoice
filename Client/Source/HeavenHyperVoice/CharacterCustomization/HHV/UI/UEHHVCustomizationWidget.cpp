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
	RebuildAppearanceOptions();
	RebuildColorOptions();
}

void UUEHHVCustomizationWidget::SetCatalog(UUEHHVCustomizationCatalog* InCatalog)
{
	Catalog = InCatalog;
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
		Entry->Label = FText::FromString(GetOptionLabel(CurrentCategory, ActualIndex));
		Entry->Icon = GetOptionIcon(CurrentCategory, ActualIndex);
		Entry->bSelected = ActualIndex == SelectedIndex;
		AppearanceEntryItems.Add(Entry);
		ListItems.Add(Entry);
	}
	AppearanceOptionList->SetListItems(ListItems);
}

void UUEHHVCustomizationWidget::RebuildColorOptions()
{
	if (!ColorOptionList)
	{
		return;
	}

	ColorEntryItems.Reset();
	TArray<UObject*> ListItems;
	const TArray<FLinearColor>* Colors = GetCatalogColors(CurrentColorChannel);
	if (Colors)
	{
		const FLinearColor SelectedColor = GetChannelColor(CurrentColorChannel);
		for (const FLinearColor& Color : *Colors)
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

		UUECharacterCreationEntryData* Entry = NewObject<UUECharacterCreationEntryData>(this);
		Entry->Kind = EUECharacterCreationEntryKind::StarterPokemon;
		Entry->StarterOwner = this;
		Entry->StarterPokemon = Pokemon;
		FString PokemonDisplayName = Pokemon->GetName();
		PokemonDisplayName.RemoveFromStart(TEXT("DA_"));
		Entry->Label = FText::FromString(PokemonDisplayName);
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
