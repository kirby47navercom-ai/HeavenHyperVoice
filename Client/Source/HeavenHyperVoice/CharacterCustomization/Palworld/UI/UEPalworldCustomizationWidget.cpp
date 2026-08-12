#include "UEPalworldCustomizationWidget.h"

#include "../Preview/UEPalworldCustomizationPreviewActor.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"

namespace
{
	const FLinearColor PanelColor(0.025f, 0.035f, 0.045f, 0.96f);
	const FLinearColor PanelSecondary(0.055f, 0.065f, 0.075f, 0.98f);
	const FLinearColor TileColor(0.095f, 0.11f, 0.125f, 1.0f);
	const FLinearColor TextColor(0.94f, 0.95f, 0.96f, 1.0f);
	const FLinearColor MutedTextColor(0.62f, 0.66f, 0.70f, 1.0f);
	const FLinearColor AccentColor(0.13f, 0.72f, 0.66f, 1.0f);
	const FLinearColor SelectedColor(0.92f, 0.55f, 0.16f, 1.0f);

	constexpr int32 ColorComponentCount = 3;

	struct FCategoryEntry
	{
		EUEPalworldCustomizationCategory Category;
		const TCHAR* Label;
	};

	TConstArrayView<FCategoryEntry> GetCategoryEntries()
	{
		static const TArray<FCategoryEntry> Entries = {
			{EUEPalworldCustomizationCategory::Body, TEXT("BODY")},
			{EUEPalworldCustomizationCategory::Head, TEXT("FACE")},
			{EUEPalworldCustomizationCategory::Hair, TEXT("HAIR")},
			{EUEPalworldCustomizationCategory::Eyes, TEXT("EYES")},
			{EUEPalworldCustomizationCategory::BodyEquipment, TEXT("OUTFIT")}
		};
		return Entries;
	}

	int32 ColorInputIndex(EUEPalworldColorChannel Channel, int32 Component)
	{
		return static_cast<int32>(Channel) * ColorComponentCount + Component;
	}
}

void UUEPalworldOptionButton::Configure(
	UUEPalworldCustomizationWidget* InOwner,
	EUEPalworldCustomizationCategory InCategory,
	int32 InIndex)
{
	OwnerWidget = InOwner;
	Category = InCategory;
	Index = InIndex;
	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UUEPalworldOptionButton::HandleClicked()
{
	if (!OwnerWidget)
	{
		return;
	}

	if (Index == INDEX_NONE)
	{
		OwnerWidget->OpenCategory(Category);
	}
	else
	{
		OwnerWidget->SelectOption(Category, Index);
	}
}

void UUEPalworldColorSpinBox::Configure(
	UUEPalworldCustomizationWidget* InOwner,
	EUEPalworldColorChannel InChannel,
	int32 InComponent)
{
	OwnerWidget = InOwner;
	Channel = InChannel;
	Component = InComponent;
	SetMinValue(0.0f);
	SetMaxValue(255.0f);
	SetMinSliderValue(0.0f);
	SetMaxSliderValue(255.0f);
	SetDelta(1.0f);
	SetAlwaysUsesDeltaSnap(true);
	SetMinFractionalDigits(0);
	SetMaxFractionalDigits(0);
	OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleValueChanged);
}

void UUEPalworldColorSpinBox::HandleValueChanged(float NewValue)
{
	if (OwnerWidget)
	{
		OwnerWidget->SetColorComponent(Channel, Component, FMath::RoundToFloat(NewValue));
	}
}

void UUEPalworldScaleSlider::Configure(UUEPalworldCustomizationWidget* InOwner, EUEPalworldScaleChannel InChannel)
{
	OwnerWidget = InOwner;
	Channel = InChannel;
	SetMinValue(0.75f);
	SetMaxValue(1.25f);
	OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleValueChanged);
}

void UUEPalworldScaleSlider::HandleValueChanged(float NewValue)
{
	if (OwnerWidget)
	{
		OwnerWidget->SetScaleFromSlider(Channel, NewValue);
	}
}

void UUEPalworldCustomizationWidget::SetPreviewActor(AUEPalworldCustomizationPreviewActor* InPreviewActor)
{
	PreviewActor = InPreviewActor;
	if (PreviewActor && !Catalog)
	{
		Catalog = PreviewActor->GetCatalog();
	}
	RefreshFromPreview();
	RebuildCategories();
	RebuildOptions();
	SynchronizeControls();
}

void UUEPalworldCustomizationWidget::SetCatalog(UUEPalworldCustomizationCatalog* InCatalog)
{
	Catalog = InCatalog;
	RebuildCategories();
	RebuildOptions();
}

void UUEPalworldCustomizationWidget::SelectOption(EUEPalworldCustomizationCategory Category, int32 Index)
{
	if (PreviewActor)
	{
		PreviewActor->SelectOption(Category, Index);
		RefreshFromPreview();
	}
	RebuildCategories();
	RebuildOptions();
	SynchronizeControls();
}

void UUEPalworldCustomizationWidget::SelectGender(EUEPalworldGender Gender)
{
	if (PreviewActor)
	{
		PreviewActor->SelectGender(Gender);
		RefreshFromPreview();
	}
	RebuildOptions();
	SynchronizeControls();
}

void UUEPalworldCustomizationWidget::SelectColor(EUEPalworldColorChannel Channel, FLinearColor Color)
{
	if (PreviewActor)
	{
		PreviewActor->SetColor(Channel, Color);
		RefreshFromPreview();
	}
	SynchronizeControls();
}

void UUEPalworldCustomizationWidget::SetScaleValue(EUEPalworldScaleChannel Channel, float Value)
{
	if (PreviewActor)
	{
		PreviewActor->SetScaleValue(Channel, Value);
		RefreshFromPreview();
	}
	SynchronizeControls();
}

void UUEPalworldCustomizationWidget::OpenCategory(EUEPalworldCustomizationCategory Category)
{
	CurrentCategory = Category;
	RebuildCategories();
	RebuildOptions();
}

void UUEPalworldCustomizationWidget::SetColorComponent(
	EUEPalworldColorChannel Channel,
	int32 Component,
	float Value)
{
	if (bSynchronizingControls || Component < 0 || Component > 2)
	{
		return;
	}

	FLinearColor Color = GetChannelColor(Channel);
	const float NormalizedValue = FMath::Clamp(FMath::RoundToFloat(Value), 0.0f, 255.0f) / 255.0f;
	if (Component == 0)
	{
		Color.R = NormalizedValue;
	}
	else if (Component == 1)
	{
		Color.G = NormalizedValue;
	}
	else
	{
		Color.B = NormalizedValue;
	}
	Color.A = 1.0f;
	SelectColor(Channel, Color);
}

void UUEPalworldCustomizationWidget::SetScaleFromSlider(EUEPalworldScaleChannel Channel, float Value)
{
	if (!bSynchronizingControls)
	{
		SetScaleValue(Channel, Value);
	}
}

int32 UUEPalworldCustomizationWidget::GetOptionCount(EUEPalworldCustomizationCategory Category) const
{
	return Catalog ? Catalog->GetOptionCount(Category) : 0;
}

FString UUEPalworldCustomizationWidget::GetOptionLabel(EUEPalworldCustomizationCategory Category, int32 Index) const
{
	if (!Catalog)
	{
		return FString();
	}

	const FUEPalworldCustomizationOption& Option = Catalog->GetOption(Category, Index);
	if (Category == EUEPalworldCustomizationCategory::Body)
	{
		if (Option.Id.Equals(TEXT("TypeA"), ESearchCase::IgnoreCase))
		{
			return TEXT("Type 1");
		}
		if (Option.Id.Equals(TEXT("TypeB"), ESearchCase::IgnoreCase))
		{
			return TEXT("Type 2");
		}
	}
	return Option.DisplayName.IsEmpty() ? Option.Id : Option.DisplayName;
}

UTexture2D* UUEPalworldCustomizationWidget::GetOptionIcon(EUEPalworldCustomizationCategory Category, int32 Index) const
{
	if (!Catalog)
	{
		return nullptr;
	}

	return Catalog->GetOption(Category, Index).Icon;
}

USkeletalMesh* UUEPalworldCustomizationWidget::GetOptionMesh(EUEPalworldCustomizationCategory Category, int32 Index) const
{
	if (!Catalog)
	{
		return nullptr;
	}

	const EUEPalworldGender Gender = PreviewActor ? PreviewActor->GetAppearance().Gender : CachedAppearance.Gender;
	return Catalog->GetOption(Category, Index).LoadMesh(Gender);
}

const FUEPalworldAppearance& UUEPalworldCustomizationWidget::GetAppearance() const
{
	return PreviewActor ? PreviewActor->GetAppearance() : CachedAppearance;
}

void UUEPalworldCustomizationWidget::RefreshFromPreview()
{
	if (PreviewActor)
	{
		CachedAppearance = PreviewActor->GetAppearance();
	}
}

TSharedRef<SWidget> UUEPalworldCustomizationWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("PalworldCustomizationWidgetTree"));
	}
	if (!WidgetTree->RootWidget || WidgetTree->RootWidget->GetName().Equals(TEXT("PalworldDesignerCanvas")))
	{
		BuildInterface();
	}
	return Super::RebuildWidget();
}

void UUEPalworldCustomizationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromPreview();
	RebuildCategories();
	RebuildOptions();
	SynchronizeControls();
}

void UUEPalworldCustomizationWidget::BuildInterface()
{
	const TGuardValue<bool> SynchronizeGuard(bSynchronizingControls, true);

	UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Root)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PalworldCustomizationRoot"));
		WidgetTree->RootWidget = Root;
	}
	Root->ClearChildren();

	ColorInputs.Empty();
	ScaleSliders.Empty();

	UTextBlock* Title = CreateText(TEXT("CHARACTER CREATION"), 25, TextColor);
	Root->AddChild(Title);
	CastChecked<UCanvasPanelSlot>(Title->Slot)->SetPosition(FVector2D(20.0f, 18.0f));
	CastChecked<UCanvasPanelSlot>(Title->Slot)->SetSize(FVector2D(700.0f, 38.0f));

	UTextBlock* Subtitle = CreateText(TEXT("Preset and custom item workflow"), 13, MutedTextColor);
	Root->AddChild(Subtitle);
	CastChecked<UCanvasPanelSlot>(Subtitle->Slot)->SetPosition(FVector2D(22.0f, 52.0f));
	CastChecked<UCanvasPanelSlot>(Subtitle->Slot)->SetSize(FVector2D(700.0f, 24.0f));

	UHorizontalBox* MainLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MainLayout"));
	Root->AddChild(MainLayout);
	UCanvasPanelSlot* MainSlot = CastChecked<UCanvasPanelSlot>(MainLayout->Slot);
	MainSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	MainSlot->SetOffsets(FMargin(20.0f, 84.0f, 20.0f, 24.0f));

	USizeBox* EditorSize = WidgetTree->ConstructWidget<USizeBox>();
	EditorSize->SetWidthOverride(630.0f);
	MainLayout->AddChild(EditorSize);

	UBorder* EditorPanel = WidgetTree->ConstructWidget<UBorder>();
	EditorPanel->SetBrushColor(PanelColor);
	EditorPanel->SetPadding(FMargin(10.0f));
	EditorSize->AddChild(EditorPanel);

	UVerticalBox* EditorColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	EditorPanel->AddChild(EditorColumn);

	UHorizontalBox* EditorBody = WidgetTree->ConstructWidget<UHorizontalBox>();
	EditorColumn->AddChild(EditorBody);
	CastChecked<UVerticalBoxSlot>(EditorBody->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	USizeBox* CategorySize = WidgetTree->ConstructWidget<USizeBox>();
	CategorySize->SetWidthOverride(138.0f);
	EditorBody->AddChild(CategorySize);
	UBorder* CategoryPanel = WidgetTree->ConstructWidget<UBorder>();
	CategoryPanel->SetBrushColor(PanelSecondary);
	CategoryPanel->SetPadding(FMargin(8.0f));
	CategorySize->AddChild(CategoryPanel);
	CategoryList = WidgetTree->ConstructWidget<UVerticalBox>();
	CategoryPanel->AddChild(CategoryList);

	USpacer* Gap = WidgetTree->ConstructWidget<USpacer>();
	Gap->SetSize(FVector2D(8.0f, 1.0f));
	EditorBody->AddChild(Gap);

	UVerticalBox* CatalogColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	EditorBody->AddChild(CatalogColumn);
	CastChecked<UHorizontalBoxSlot>(CatalogColumn->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	OptionTitle = CreateText(GetCategoryLabel(CurrentCategory), 24, TextColor);
	CatalogColumn->AddChild(OptionTitle);
	OptionCount = CreateText(TEXT("0 Palworld table items"), 13, MutedTextColor);
	CatalogColumn->AddChild(OptionCount);

	UScrollBox* CatalogScroll = WidgetTree->ConstructWidget<UScrollBox>();
	CatalogColumn->AddChild(CatalogScroll);
	CastChecked<UVerticalBoxSlot>(CatalogScroll->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	OptionGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	CatalogScroll->AddChild(OptionGrid);

	USpacer* CenterSpace = WidgetTree->ConstructWidget<USpacer>();
	CenterSpace->SetSize(FVector2D(1.0f, 1.0f));
	MainLayout->AddChild(CenterSpace);
	CastChecked<UHorizontalBoxSlot>(CenterSpace->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	USizeBox* ParameterSize = WidgetTree->ConstructWidget<USizeBox>();
	ParameterSize->SetWidthOverride(330.0f);
	MainLayout->AddChild(ParameterSize);
	UBorder* ParameterPanel = WidgetTree->ConstructWidget<UBorder>();
	ParameterPanel->SetBrushColor(PanelColor);
	ParameterPanel->SetPadding(FMargin(14.0f));
	ParameterSize->AddChild(ParameterPanel);

	UScrollBox* ParameterScroll = WidgetTree->ConstructWidget<UScrollBox>();
	ParameterPanel->AddChild(ParameterScroll);
	UVerticalBox* Parameters = WidgetTree->ConstructWidget<UVerticalBox>();
	ParameterScroll->AddChild(Parameters);

	AddSectionTitle(Parameters, TEXT("RGB 0-255"));
	AddRGBRow(Parameters, TEXT("Skin"), EUEPalworldColorChannel::Skin);
	AddRGBRow(Parameters, TEXT("Hair"), EUEPalworldColorChannel::Hair);
	AddRGBRow(Parameters, TEXT("Eyes"), EUEPalworldColorChannel::Eye);
	AddRGBRow(Parameters, TEXT("Outfit"), EUEPalworldColorChannel::BodyEquipment);

	AddSectionTitle(Parameters, TEXT("Scale"));
	AddScaleRow(Parameters, TEXT("Height"), EUEPalworldScaleChannel::Height);
	AddScaleRow(Parameters, TEXT("Head Size"), EUEPalworldScaleChannel::HeadSize);
	AddScaleRow(Parameters, TEXT("Body Width"), EUEPalworldScaleChannel::BodyWidth);

	StatusText = CreateText(TEXT("Select an item"), 12, MutedTextColor);
	StatusText->SetAutoWrapText(true);
	Parameters->AddChild(StatusText);
	CastChecked<UVerticalBoxSlot>(StatusText->Slot)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	RebuildCategories();
	RebuildOptions();
	SynchronizeControls();
}

void UUEPalworldCustomizationWidget::RebuildCategories()
{
	if (!CategoryList)
	{
		return;
	}
	CategoryList->ClearChildren();

	for (const FCategoryEntry& Entry : GetCategoryEntries())
	{
		const int32 Count = GetVisibleOptionCount(Entry.Category);
		UUEPalworldOptionButton* Button = WidgetTree->ConstructWidget<UUEPalworldOptionButton>();
		Button->Configure(this, Entry.Category, INDEX_NONE);
		Button->SetBackgroundColor(Entry.Category == CurrentCategory ? AccentColor : TileColor);

		UTextBlock* Label = CreateText(
			FString::Printf(TEXT("%s  %d"), Entry.Label, Count),
			12,
			TextColor);
		Label->SetAutoWrapText(true);
		Button->AddChild(Label);
		CategoryList->AddChild(Button);
		CastChecked<UVerticalBoxSlot>(Button->Slot)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	}
}

void UUEPalworldCustomizationWidget::RebuildOptions()
{
	if (!OptionGrid || !OptionTitle || !OptionCount)
	{
		return;
	}

	OptionGrid->ClearChildren();
	const int32 Count = GetVisibleOptionCount(CurrentCategory);
	OptionTitle->SetText(FText::FromString(GetCategoryLabel(CurrentCategory)));
	OptionCount->SetText(FText::FromString(FString::Printf(TEXT("%d Palworld table items"), Count)));

	const int32 SelectedIndex = GetSelectedIndex(CurrentCategory);
	for (int32 VisibleIndex = 0; VisibleIndex < Count; ++VisibleIndex)
	{
		const int32 Index = GetActualOptionIndex(CurrentCategory, VisibleIndex);
		const FString OptionLabel = GetOptionLabel(CurrentCategory, Index);
		USizeBox* TileSize = WidgetTree->ConstructWidget<USizeBox>();
		TileSize->SetWidthOverride(132.0f);
		TileSize->SetHeightOverride(152.0f);
		TileSize->SetClipping(EWidgetClipping::ClipToBounds);
		OptionGrid->AddChildToUniformGrid(TileSize, VisibleIndex / 3, VisibleIndex % 3);

		UUEPalworldOptionButton* Button = WidgetTree->ConstructWidget<UUEPalworldOptionButton>();
		Button->Configure(this, CurrentCategory, Index);
		Button->SetBackgroundColor(Index == SelectedIndex ? SelectedColor : TileColor);
		Button->SetClipping(EWidgetClipping::ClipToBounds);
		TileSize->AddChild(Button);

		UVerticalBox* Tile = WidgetTree->ConstructWidget<UVerticalBox>();
		Button->AddChild(Tile);

		USizeBox* PreviewSize = WidgetTree->ConstructWidget<USizeBox>();
		PreviewSize->SetWidthOverride(122.0f);
		PreviewSize->SetHeightOverride(92.0f);
		UBorder* PreviewFrame = WidgetTree->ConstructWidget<UBorder>();
		PreviewFrame->SetBrushColor(PanelSecondary);
		PreviewFrame->SetPadding(FMargin(3.0f));
		PreviewSize->AddChild(PreviewFrame);
		Tile->AddChild(PreviewSize);

		if (UTexture2D* Icon = GetOptionIcon(CurrentCategory, Index))
		{
			UImage* Image = WidgetTree->ConstructWidget<UImage>();
			Image->SetBrushFromTexture(Icon, true);
			Image->SetColorAndOpacity(FLinearColor::White);
			PreviewFrame->AddChild(Image);
		}
		else
		{
			const FString MeshName = GetOptionMesh(CurrentCategory, Index)
				? GetOptionMesh(CurrentCategory, Index)->GetName()
				: OptionLabel;
			UTextBlock* MeshLabel = CreateText(MeshName, 9, MutedTextColor);
			MeshLabel->SetAutoWrapText(true);
			MeshLabel->SetJustification(ETextJustify::Center);
			PreviewFrame->AddChild(MeshLabel);
		}

		UTextBlock* Label = CreateText(
			FString::Printf(TEXT("#%02d  %s"), VisibleIndex + 1, *OptionLabel),
			11,
			TextColor);
		Label->SetAutoWrapText(true);
		Label->SetJustification(ETextJustify::Center);
		Tile->AddChild(Label);
		CastChecked<UVerticalBoxSlot>(Label->Slot)->SetPadding(FMargin(2.0f, 4.0f, 2.0f, 0.0f));
	}

	if (Count == 0)
	{
		UTextBlock* Empty = CreateText(TEXT("No extracted item in this category."), 13, MutedTextColor);
		Empty->SetAutoWrapText(true);
		OptionGrid->AddChildToUniformGrid(Empty, 0, 0);
	}
}

void UUEPalworldCustomizationWidget::SynchronizeControls()
{
	const TGuardValue<bool> SynchronizeGuard(bSynchronizingControls, true);
	RefreshFromPreview();

	for (const TObjectPtr<UUEPalworldColorSpinBox>& Input : ColorInputs)
	{
		if (!Input)
		{
			continue;
		}
		const FLinearColor Color = GetChannelColor(Input->GetChannel()).GetClamped();
		float ComponentValue = Color.R;
		if (Input->GetComponent() == 1)
		{
			ComponentValue = Color.G;
		}
		else if (Input->GetComponent() == 2)
		{
			ComponentValue = Color.B;
		}
		Input->SetValue(FMath::RoundToFloat(ComponentValue * 255.0f));
	}

	for (const TObjectPtr<UUEPalworldScaleSlider>& Slider : ScaleSliders)
	{
		if (Slider)
		{
			Slider->SetValue(GetScaleValue(Slider->GetChannel()));
		}
	}

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(
			TEXT("Type %s | %s #%02d"),
			CachedAppearance.Gender == EUEPalworldGender::TypeA ? TEXT("A") : TEXT("B"),
			*GetCategoryLabel(CurrentCategory),
			GetSelectedIndex(CurrentCategory) + 1)));
	}
}

int32 UUEPalworldCustomizationWidget::GetSelectedIndex(EUEPalworldCustomizationCategory Category) const
{
	const FUEPalworldAppearance& Appearance = GetAppearance();
	switch (Category)
	{
	case EUEPalworldCustomizationCategory::Body:
		return Appearance.BodyIndex;
	case EUEPalworldCustomizationCategory::Head:
		return Appearance.HeadIndex;
	case EUEPalworldCustomizationCategory::Hair:
		return Appearance.HairIndex;
	case EUEPalworldCustomizationCategory::Eyes:
		return Appearance.EyeIndex;
	case EUEPalworldCustomizationCategory::BodyEquipment:
		return Appearance.BodyEquipmentIndex;
	case EUEPalworldCustomizationCategory::HeadEquipment:
		return Appearance.HeadEquipmentIndex;
	default:
		return 0;
	}
}

int32 UUEPalworldCustomizationWidget::GetVisibleOptionCount(EUEPalworldCustomizationCategory Category) const
{
	const int32 RawCount = GetOptionCount(Category);
	if (Category == EUEPalworldCustomizationCategory::HeadEquipment)
	{
		return 0;
	}
	if (Category == EUEPalworldCustomizationCategory::Body && RawCount > 2)
	{
		return 2;
	}
	return RawCount;
}

int32 UUEPalworldCustomizationWidget::GetActualOptionIndex(
	EUEPalworldCustomizationCategory Category,
	int32 VisibleIndex) const
{
	if (Category == EUEPalworldCustomizationCategory::HeadEquipment)
	{
		return -1;
	}
	if (Category == EUEPalworldCustomizationCategory::Body && GetOptionCount(Category) > 2)
	{
		return FMath::Clamp(VisibleIndex + 1, 1, GetOptionCount(Category) - 1);
	}
	return VisibleIndex;
}

FLinearColor UUEPalworldCustomizationWidget::GetChannelColor(EUEPalworldColorChannel Channel) const
{
	const FUEPalworldAppearance& Appearance = GetAppearance();
	switch (Channel)
	{
	case EUEPalworldColorChannel::Skin:
		return Appearance.SkinColor;
	case EUEPalworldColorChannel::Hair:
		return Appearance.HairColor;
	case EUEPalworldColorChannel::Eye:
		return Appearance.EyeColor;
	case EUEPalworldColorChannel::BodyEquipment:
		return Appearance.BodyEquipmentColor;
	case EUEPalworldColorChannel::HeadEquipment:
		return Appearance.HeadEquipmentColor;
	default:
		return FLinearColor::White;
	}
}

float UUEPalworldCustomizationWidget::GetScaleValue(EUEPalworldScaleChannel Channel) const
{
	const FUEPalworldAppearance& Appearance = GetAppearance();
	switch (Channel)
	{
	case EUEPalworldScaleChannel::Height:
		return Appearance.HeightScale;
	case EUEPalworldScaleChannel::HeadSize:
		return Appearance.HeadScale;
	case EUEPalworldScaleChannel::BodyWidth:
		return Appearance.BodyWidthScale;
	default:
		return 1.0f;
	}
}

UTextBlock* UUEPalworldCustomizationWidget::CreateText(
	const FString& Text,
	int32 FontSize,
	const FLinearColor& Color)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>();
	TextBlock->SetText(FText::FromString(Text));
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), FontSize));
	return TextBlock;
}

UButton* UUEPalworldCustomizationWidget::CreateTextButton(const FString& Text)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Button->SetBackgroundColor(TileColor);
	Button->AddChild(CreateText(Text, 12, TextColor));
	return Button;
}

void UUEPalworldCustomizationWidget::AddSectionTitle(UVerticalBox* Parent, const FString& Text)
{
	UTextBlock* Title = CreateText(Text, 17, AccentColor);
	Parent->AddChild(Title);
	CastChecked<UVerticalBoxSlot>(Title->Slot)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 6.0f));
}

void UUEPalworldCustomizationWidget::AddInfoLine(UVerticalBox* Parent, const FString& Text)
{
	UTextBlock* Line = CreateText(Text, 12, MutedTextColor);
	Line->SetAutoWrapText(true);
	Parent->AddChild(Line);
	CastChecked<UVerticalBoxSlot>(Line->Slot)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
}

void UUEPalworldCustomizationWidget::AddRGBRow(
	UVerticalBox* Parent,
	const FString& Label,
	EUEPalworldColorChannel Channel)
{
	UTextBlock* RowLabel = CreateText(Label, 12, TextColor);
	Parent->AddChild(RowLabel);
	CastChecked<UVerticalBoxSlot>(RowLabel->Slot)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 2.0f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChild(Row);
	CastChecked<UVerticalBoxSlot>(Row->Slot)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));

	const TCHAR* Labels[] = {TEXT("R"), TEXT("G"), TEXT("B")};
	for (int32 Component = 0; Component < ColorComponentCount; ++Component)
	{
		UTextBlock* ComponentLabel = CreateText(Labels[Component], 11, MutedTextColor);
		Row->AddChild(ComponentLabel);
		CastChecked<UHorizontalBoxSlot>(ComponentLabel->Slot)->SetPadding(FMargin(0.0f, 5.0f, 3.0f, 0.0f));

		UUEPalworldColorSpinBox* Input = WidgetTree->ConstructWidget<UUEPalworldColorSpinBox>();
		Input->Configure(this, Channel, Component);
		USizeBox* InputSize = WidgetTree->ConstructWidget<USizeBox>();
		InputSize->SetWidthOverride(58.0f);
		InputSize->AddChild(Input);
		Row->AddChild(InputSize);
		CastChecked<UHorizontalBoxSlot>(InputSize->Slot)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

		const int32 RequiredSize = ColorInputIndex(Channel, Component) + 1;
		if (ColorInputs.Num() < RequiredSize)
		{
			ColorInputs.SetNum(RequiredSize);
		}
		ColorInputs[ColorInputIndex(Channel, Component)] = Input;
	}
}

void UUEPalworldCustomizationWidget::AddScaleRow(
	UVerticalBox* Parent,
	const FString& Label,
	EUEPalworldScaleChannel Channel)
{
	UTextBlock* RowLabel = CreateText(Label, 12, TextColor);
	Parent->AddChild(RowLabel);
	CastChecked<UVerticalBoxSlot>(RowLabel->Slot)->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 2.0f));

	UUEPalworldScaleSlider* Slider = WidgetTree->ConstructWidget<UUEPalworldScaleSlider>();
	Slider->Configure(this, Channel);
	Parent->AddChild(Slider);
	CastChecked<UVerticalBoxSlot>(Slider->Slot)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
	ScaleSliders.Add(Slider);
}

FString UUEPalworldCustomizationWidget::GetCategoryLabel(EUEPalworldCustomizationCategory Category)
{
	switch (Category)
	{
	case EUEPalworldCustomizationCategory::Body:
		return TEXT("Body");
	case EUEPalworldCustomizationCategory::Head:
		return TEXT("Face");
	case EUEPalworldCustomizationCategory::Hair:
		return TEXT("Hair");
	case EUEPalworldCustomizationCategory::Eyes:
		return TEXT("Eyes");
	case EUEPalworldCustomizationCategory::BodyEquipment:
		return TEXT("Outfit");
	case EUEPalworldCustomizationCategory::HeadEquipment:
		return TEXT("Disabled");
	default:
		return TEXT("Items");
	}
}
