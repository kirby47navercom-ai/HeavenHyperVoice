#include "UECustomizationWidget.h"

#include "../Preview/UECustomizationPreviewActor.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor PanelColor(0.025f, 0.035f, 0.045f, 0.96f);
	const FLinearColor PanelSecondary(0.055f, 0.065f, 0.075f, 0.98f);
	const FLinearColor TextColor(0.94f, 0.95f, 0.96f, 1.0f);
	const FLinearColor MutedTextColor(0.62f, 0.66f, 0.70f, 1.0f);
	const FLinearColor AccentColor(0.13f, 0.72f, 0.66f, 1.0f);
	const FLinearColor SelectedColor(0.92f, 0.55f, 0.16f, 1.0f);

	struct FCategoryEntry
	{
		EUECustomizationPart Part;
		const TCHAR* Label;
	};
}

void UUECustomizationOptionButton::Configure(
	UUECustomizationWidget* InOwner,
	EUECustomizationPart InPart,
	int32 InIndex)
{
	OwnerWidget = InOwner;
	Part = InPart;
	Index = InIndex;
	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UUECustomizationOptionButton::HandleClicked()
{
	if (!OwnerWidget)
	{
		return;
	}
	if (Index == INDEX_NONE)
	{
		OwnerWidget->OpenCategory(Part);
	}
	else
	{
		OwnerWidget->SelectPartOption(Part, Index);
	}
}

void UUECustomizationWidget::SetPreviewActor(AUECustomizationPreviewActor* InPreviewActor)
{
	PreviewActor = InPreviewActor;
	if (PreviewActor)
	{
		PreviewActor->InitializeCatalogs();
	}
	SynchronizeControls();
}

TSharedRef<SWidget> UUECustomizationWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("CustomizationWidgetTree"));
	}
	if (!WidgetTree->RootWidget)
	{
		BuildInterface();
	}
	return Super::RebuildWidget();
}

void UUECustomizationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SynchronizeControls();
}

void UUECustomizationWidget::BuildInterface()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CustomizationRoot"));
	WidgetTree->RootWidget = Root;

	UTextBlock* Title = CreateText(TEXT("VRoid CHARACTER EDITOR"), 27, TextColor);
	Root->AddChild(Title);
	CastChecked<UCanvasPanelSlot>(Title->Slot)->SetPosition(FVector2D(26.0f, 18.0f));
	CastChecked<UCanvasPanelSlot>(Title->Slot)->SetSize(FVector2D(620.0f, 40.0f));

	UTextBlock* Subtitle = CreateText(TEXT("Preset and custom-item workflow"), 13, MutedTextColor);
	Root->AddChild(Subtitle);
	CastChecked<UCanvasPanelSlot>(Subtitle->Slot)->SetPosition(FVector2D(28.0f, 54.0f));
	CastChecked<UCanvasPanelSlot>(Subtitle->Slot)->SetSize(FVector2D(620.0f, 24.0f));

	UHorizontalBox* MainLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MainLayout"));
	Root->AddChild(MainLayout);
	UCanvasPanelSlot* MainSlot = CastChecked<UCanvasPanelSlot>(MainLayout->Slot);
	MainSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	MainSlot->SetOffsets(FMargin(24.0f, 88.0f, 24.0f, 24.0f));

	USizeBox* EditorSize = WidgetTree->ConstructWidget<USizeBox>();
	EditorSize->SetWidthOverride(620.0f);
	MainLayout->AddChild(EditorSize);
	UBorder* EditorPanel = WidgetTree->ConstructWidget<UBorder>();
	EditorPanel->SetBrushColor(PanelColor);
	EditorPanel->SetPadding(FMargin(12.0f));
	EditorSize->AddChild(EditorPanel);

	UVerticalBox* EditorColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	EditorPanel->AddChild(EditorColumn);
	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	EditorColumn->AddChild(Tabs);
	for (const FString& TabName : {TEXT("BODY"), TEXT("FACE"), TEXT("HAIR"), TEXT("OUTFIT"), TEXT("ACCESSORY")})
	{
		UButton* Tab = CreateTextButton(TabName);
		Tabs->AddChild(Tab);
		CastChecked<UHorizontalBoxSlot>(Tab->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CastChecked<UHorizontalBoxSlot>(Tab->Slot)->SetPadding(FMargin(3.0f));
		if (TabName == TEXT("BODY")) Tab->OnClicked.AddDynamic(this, &ThisClass::ShowBody);
		else if (TabName == TEXT("FACE")) Tab->OnClicked.AddDynamic(this, &ThisClass::ShowFace);
		else if (TabName == TEXT("HAIR")) Tab->OnClicked.AddDynamic(this, &ThisClass::ShowHairstyle);
		else if (TabName == TEXT("OUTFIT")) Tab->OnClicked.AddDynamic(this, &ThisClass::ShowOutfit);
		else Tab->OnClicked.AddDynamic(this, &ThisClass::ShowAccessory);
	}

	UHorizontalBox* EditorBody = WidgetTree->ConstructWidget<UHorizontalBox>();
	EditorColumn->AddChild(EditorBody);
	CastChecked<UVerticalBoxSlot>(EditorBody->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CastChecked<UVerticalBoxSlot>(EditorBody->Slot)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));

	USizeBox* CategorySize = WidgetTree->ConstructWidget<USizeBox>();
	CategorySize->SetWidthOverride(172.0f);
	EditorBody->AddChild(CategorySize);
	UBorder* CategoryPanel = WidgetTree->ConstructWidget<UBorder>();
	CategoryPanel->SetBrushColor(PanelSecondary);
	CategoryPanel->SetPadding(FMargin(8.0f));
	CategorySize->AddChild(CategoryPanel);
	UScrollBox* CategoryScroll = WidgetTree->ConstructWidget<UScrollBox>();
	CategoryPanel->AddChild(CategoryScroll);
	CategoryList = WidgetTree->ConstructWidget<UVerticalBox>();
	CategoryScroll->AddChild(CategoryList);

	USpacer* EditorGap = WidgetTree->ConstructWidget<USpacer>();
	EditorGap->SetSize(FVector2D(10.0f, 1.0f));
	EditorBody->AddChild(EditorGap);

	UVerticalBox* CatalogColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	EditorBody->AddChild(CatalogColumn);
	CastChecked<UHorizontalBoxSlot>(CatalogColumn->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CatalogTitle = CreateText(TEXT("Irises"), 20, TextColor);
	CatalogColumn->AddChild(CatalogTitle);
	CatalogCount = CreateText(TEXT("0 items"), 12, MutedTextColor);
	CatalogColumn->AddChild(CatalogCount);
	UScrollBox* CatalogScroll = WidgetTree->ConstructWidget<UScrollBox>();
	CatalogColumn->AddChild(CatalogScroll);
	CastChecked<UVerticalBoxSlot>(CatalogScroll->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CastChecked<UVerticalBoxSlot>(CatalogScroll->Slot)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	OptionGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	OptionGrid->SetSlotPadding(FMargin(4.0f));
	CatalogScroll->AddChild(OptionGrid);

	USpacer* CenterSpace = WidgetTree->ConstructWidget<USpacer>();
	MainLayout->AddChild(CenterSpace);
	CastChecked<UHorizontalBoxSlot>(CenterSpace->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	USizeBox* ParameterSize = WidgetTree->ConstructWidget<USizeBox>();
	ParameterSize->SetWidthOverride(310.0f);
	MainLayout->AddChild(ParameterSize);
	UBorder* ParameterPanel = WidgetTree->ConstructWidget<UBorder>();
	ParameterPanel->SetBrushColor(PanelColor);
	ParameterPanel->SetPadding(FMargin(16.0f));
	ParameterSize->AddChild(ParameterPanel);
	UVerticalBox* Parameters = WidgetTree->ConstructWidget<UVerticalBox>();
	ParameterPanel->AddChild(Parameters);

	AddSectionTitle(Parameters, TEXT("PARAMETERS"));
	AddSliderRow(Parameters, TEXT("Height"), HeightSlider);
	AddSliderRow(Parameters, TEXT("Head Size"), HeadSizeSlider);
	AddSliderRow(Parameters, TEXT("Shoulder / Body Width"), ShoulderSlider);
	HeightSlider->OnValueChanged.AddDynamic(this, &ThisClass::HeightChanged);
	HeadSizeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HeadSizeChanged);
	ShoulderSlider->OnValueChanged.AddDynamic(this, &ThisClass::ShoulderWidthChanged);

	TArray<UButton*> SkinButtons;
	AddColorRow(Parameters, TEXT("Skin"), {
		FLinearColor::FromSRGBColor(FColor(239, 185, 151)),
		FLinearColor::FromSRGBColor(FColor(184, 128, 92)),
		FLinearColor::FromSRGBColor(FColor(105, 66, 47))}, SkinButtons);
	SkinButtons[0]->OnClicked.AddDynamic(this, &ThisClass::SkinLight);
	SkinButtons[1]->OnClicked.AddDynamic(this, &ThisClass::SkinMedium);
	SkinButtons[2]->OnClicked.AddDynamic(this, &ThisClass::SkinDeep);

	TArray<UButton*> EyeButtons;
	AddColorRow(Parameters, TEXT("Iris"), {
		FLinearColor(0.38f, 0.16f, 0.06f),
		FLinearColor(0.12f, 0.38f, 0.72f),
		FLinearColor(0.16f, 0.42f, 0.22f)}, EyeButtons);
	EyeButtons[0]->OnClicked.AddDynamic(this, &ThisClass::EyeOriginal);
	EyeButtons[1]->OnClicked.AddDynamic(this, &ThisClass::EyeBlue);
	EyeButtons[2]->OnClicked.AddDynamic(this, &ThisClass::EyeBrown);

	TArray<UButton*> LipButtons;
	AddColorRow(Parameters, TEXT("Lips"), {
		FLinearColor::FromSRGBColor(FColor(196, 102, 116)),
		FLinearColor::FromSRGBColor(FColor(225, 154, 165)),
		FLinearColor::FromSRGBColor(FColor(133, 52, 70))}, LipButtons);
	LipButtons[0]->OnClicked.AddDynamic(this, &ThisClass::LipNatural);
	LipButtons[1]->OnClicked.AddDynamic(this, &ThisClass::LipSoft);
	LipButtons[2]->OnClicked.AddDynamic(this, &ThisClass::LipDeep);

	TArray<UButton*> HairButtons;
	AddColorRow(Parameters, TEXT("Hair"), {FLinearColor::White, FLinearColor(0.12f, 0.22f, 0.55f), FLinearColor(0.55f, 0.12f, 0.16f)}, HairButtons);
	HairButtons[0]->OnClicked.AddDynamic(this, &ThisClass::HairOriginal);
	HairButtons[1]->OnClicked.AddDynamic(this, &ThisClass::HairBlue);
	HairButtons[2]->OnClicked.AddDynamic(this, &ThisClass::HairCoral);

	TArray<UButton*> OutfitButtons;
	AddColorRow(Parameters, TEXT("Outfit"), {FLinearColor::White, FLinearColor(0.25f, 0.65f, 1.0f), FLinearColor(1.0f, 0.20f, 0.16f)}, OutfitButtons);
	OutfitButtons[0]->OnClicked.AddDynamic(this, &ThisClass::OutfitOriginal);
	OutfitButtons[1]->OnClicked.AddDynamic(this, &ThisClass::OutfitCyan);
	OutfitButtons[2]->OnClicked.AddDynamic(this, &ThisClass::OutfitRed);

	AddSectionTitle(Parameters, TEXT("PREVIEW"));
	UHorizontalBox* RotateRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parameters->AddChild(RotateRow);
	UButton* RotateLeftButton = CreateTextButton(TEXT("<"));
	UButton* RotateRightButton = CreateTextButton(TEXT(">"));
	RotateRow->AddChild(RotateLeftButton);
	RotateRow->AddChild(RotateRightButton);
	for (UWidget* Child : RotateRow->GetAllChildren())
	{
		CastChecked<UHorizontalBoxSlot>(Child->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CastChecked<UHorizontalBoxSlot>(Child->Slot)->SetPadding(FMargin(3.0f));
	}
	RotateLeftButton->OnClicked.AddDynamic(this, &ThisClass::RotateLeft);
	RotateRightButton->OnClicked.AddDynamic(this, &ThisClass::RotateRight);

	UButton* RandomButton = CreateTextButton(TEXT("RANDOMIZE"));
	Parameters->AddChild(RandomButton);
	CastChecked<UVerticalBoxSlot>(RandomButton->Slot)->SetPadding(FMargin(3.0f, 8.0f));
	RandomButton->OnClicked.AddDynamic(this, &ThisClass::Randomize);

	USpacer* FlexibleSpace = WidgetTree->ConstructWidget<USpacer>();
	Parameters->AddChild(FlexibleSpace);
	CastChecked<UVerticalBoxSlot>(FlexibleSpace->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	StatusText = CreateText(TEXT("Select a category and an item"), 12, MutedTextColor);
	Parameters->AddChild(StatusText);

	UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parameters->AddChild(ActionRow);
	UButton* ResetButton = CreateTextButton(TEXT("RESET"));
	UButton* SaveButton = CreateTextButton(TEXT("SAVE"));
	ActionRow->AddChild(ResetButton);
	ActionRow->AddChild(SaveButton);
	for (UWidget* Child : ActionRow->GetAllChildren())
	{
		CastChecked<UHorizontalBoxSlot>(Child->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CastChecked<UHorizontalBoxSlot>(Child->Slot)->SetPadding(FMargin(3.0f));
	}
	ResetButton->OnClicked.AddDynamic(this, &ThisClass::Reset);
	SaveButton->OnClicked.AddDynamic(this, &ThisClass::Save);

	SetSection(EEditorSection::Face);
}

UTextBlock* UUECustomizationWidget::CreateText(const FString& Text, int32 FontSize, const FLinearColor& Color)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>();
	TextBlock->SetText(FText::FromString(Text));
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FontSize));
	return TextBlock;
}

UButton* UUECustomizationWidget::CreateTextButton(const FString& Text)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Button->SetBackgroundColor(FLinearColor(0.10f, 0.17f, 0.21f, 1.0f));
	UTextBlock* Label = CreateText(Text, 13, TextColor);
	Label->SetJustification(ETextJustify::Center);
	Button->AddChild(Label);
	return Button;
}

void UUECustomizationWidget::AddSectionTitle(UVerticalBox* Parent, const FString& Text)
{
	UTextBlock* Section = CreateText(Text, 16, AccentColor);
	Parent->AddChild(Section);
	CastChecked<UVerticalBoxSlot>(Section->Slot)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 6.0f));
}

void UUECustomizationWidget::AddSliderRow(UVerticalBox* Parent, const FString& Label, TObjectPtr<USlider>& OutSlider)
{
	Parent->AddChild(CreateText(Label, 12, MutedTextColor));
	OutSlider = WidgetTree->ConstructWidget<USlider>();
	OutSlider->SetMinValue(0.0f);
	OutSlider->SetMaxValue(1.0f);
	OutSlider->SetStepSize(0.01f);
	OutSlider->SetSliderBarColor(FLinearColor(0.12f, 0.25f, 0.28f, 1.0f));
	OutSlider->SetSliderHandleColor(AccentColor);
	Parent->AddChild(OutSlider);
	CastChecked<UVerticalBoxSlot>(OutSlider->Slot)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 8.0f));
}

void UUECustomizationWidget::AddColorRow(
	UVerticalBox* Parent,
	const FString& Label,
	const TArray<FLinearColor>& Colors,
	TArray<UButton*>& OutButtons)
{
	Parent->AddChild(CreateText(Label, 12, MutedTextColor));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChild(Row);
	for (const FLinearColor& Color : Colors)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(54.0f);
		Size->SetHeightOverride(30.0f);
		Row->AddChild(Size);
		CastChecked<UHorizontalBoxSlot>(Size->Slot)->SetPadding(FMargin(3.0f));
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		Button->SetBackgroundColor(Color);
		Size->AddChild(Button);
		OutButtons.Add(Button);
	}
}

void UUECustomizationWidget::SetSection(EEditorSection Section)
{
	CurrentSection = Section;
	switch (CurrentSection)
	{
	case EEditorSection::Body: CurrentPart = EUECustomizationPart::Gender; break;
	case EEditorSection::Face: CurrentPart = EUECustomizationPart::EyeIris; break;
	case EEditorSection::Hairstyle: CurrentPart = EUECustomizationPart::HairSet; break;
	case EEditorSection::Outfit: CurrentPart = EUECustomizationPart::Top; break;
	case EEditorSection::Accessory: CurrentPart = EUECustomizationPart::HeadAccessory; break;
	}
	RebuildCategories();
	RebuildCatalog();
}

void UUECustomizationWidget::RebuildCategories()
{
	if (!CategoryList)
	{
		return;
	}
	CategoryList->ClearChildren();
	TArray<FCategoryEntry> Entries;
	switch (CurrentSection)
	{
	case EEditorSection::Body:
		Entries = {{EUECustomizationPart::Gender, TEXT("Base Model")}, {EUECustomizationPart::Body, TEXT("Body Type")}};
		break;
	case EEditorSection::Face:
		Entries = {
			{EUECustomizationPart::FaceSkin, TEXT("Skin")},
			{EUECustomizationPart::EyeIris, TEXT("Irises")},
			{EUECustomizationPart::EyeHighlight, TEXT("Eye Highlights")},
			{EUECustomizationPart::EyeWhite, TEXT("Scleras")},
			{EUECustomizationPart::EyeExtra, TEXT("Eyelids")},
			{EUECustomizationPart::Brow, TEXT("Eyebrows")},
			{EUECustomizationPart::Eyelash, TEXT("Eyelashes")},
			{EUECustomizationPart::Eyeline, TEXT("Eyeliner")},
			{EUECustomizationPart::Mouth, TEXT("Mouth Inside")},
			{EUECustomizationPart::Lip, TEXT("Lips")},
			{EUECustomizationPart::MouthLine, TEXT("Mouth Line")}};
		break;
	case EEditorSection::Hairstyle:
		Entries = {
			{EUECustomizationPart::HairSet, TEXT("Hair Sets")},
			{EUECustomizationPart::HairFront, TEXT("Front")},
			{EUECustomizationPart::HairBack, TEXT("Back")},
			{EUECustomizationPart::HairSide, TEXT("Side")},
			{EUECustomizationPart::HairExtra, TEXT("Extensions / Ahoge / Extra")},
			{EUECustomizationPart::HairBase, TEXT("Base Hair")}};
		break;
	case EEditorSection::Outfit:
		Entries = {
			{EUECustomizationPart::Top, TEXT("Tops")},
			{EUECustomizationPart::Bottom, TEXT("Bottoms")},
			{EUECustomizationPart::Onepiece, TEXT("Dresses")},
			{EUECustomizationPart::Shoes, TEXT("Shoes")}};
		break;
	case EEditorSection::Accessory:
		Entries = {
			{EUECustomizationPart::HeadAccessory, TEXT("Head")},
			{EUECustomizationPart::FaceAccessory, TEXT("Face / Glasses")},
			{EUECustomizationPart::EarAccessory, TEXT("Ears")},
			{EUECustomizationPart::TailAccessory, TEXT("Tails")},
			{EUECustomizationPart::NeckAccessory, TEXT("Neck")}};
		break;
	}

	for (const FCategoryEntry& Entry : Entries)
	{
		UUECustomizationOptionButton* Button = WidgetTree->ConstructWidget<UUECustomizationOptionButton>();
		Button->Configure(this, Entry.Part, INDEX_NONE);
		Button->SetBackgroundColor(Entry.Part == CurrentPart ? AccentColor : FLinearColor(0.08f, 0.10f, 0.12f, 1.0f));
		UTextBlock* Label = CreateText(Entry.Label, 13, TextColor);
		Button->AddChild(Label);
		CategoryList->AddChild(Button);
		CastChecked<UVerticalBoxSlot>(Button->Slot)->SetPadding(FMargin(0.0f, 2.0f));
	}
}

void UUECustomizationWidget::OpenCategory(EUECustomizationPart Part)
{
	CurrentPart = Part;
	RebuildCategories();
	RebuildCatalog();
}

void UUECustomizationWidget::RebuildCatalog()
{
	if (!OptionGrid || !CatalogTitle || !CatalogCount)
	{
		return;
	}
	OptionGrid->ClearChildren();
	const int32 Count = PreviewActor ? PreviewActor->GetOptionCount(CurrentPart) : 0;
	FString CatalogCategoryLabel = TEXT("Items");
	switch (CurrentPart)
	{
	case EUECustomizationPart::Gender: CatalogCategoryLabel = TEXT("Base Model"); break;
	case EUECustomizationPart::Body: CatalogCategoryLabel = TEXT("Body Type"); break;
	case EUECustomizationPart::FaceSkin: CatalogCategoryLabel = TEXT("Skin"); break;
	case EUECustomizationPart::EyeIris: CatalogCategoryLabel = TEXT("Irises"); break;
	case EUECustomizationPart::EyeHighlight: CatalogCategoryLabel = TEXT("Eye Highlights"); break;
	case EUECustomizationPart::EyeWhite: CatalogCategoryLabel = TEXT("Scleras"); break;
	case EUECustomizationPart::EyeExtra: CatalogCategoryLabel = TEXT("Eyelids"); break;
	case EUECustomizationPart::Brow: CatalogCategoryLabel = TEXT("Eyebrows"); break;
	case EUECustomizationPart::Eyelash: CatalogCategoryLabel = TEXT("Eyelashes"); break;
	case EUECustomizationPart::Eyeline: CatalogCategoryLabel = TEXT("Eyeliner"); break;
	case EUECustomizationPart::Mouth: CatalogCategoryLabel = TEXT("Mouth Inside"); break;
	case EUECustomizationPart::Lip: CatalogCategoryLabel = TEXT("Lips"); break;
	case EUECustomizationPart::MouthLine: CatalogCategoryLabel = TEXT("Mouth Line"); break;
	case EUECustomizationPart::HairSet: CatalogCategoryLabel = TEXT("Hair Sets"); break;
	case EUECustomizationPart::HairFront: CatalogCategoryLabel = TEXT("Front Hair"); break;
	case EUECustomizationPart::HairBack: CatalogCategoryLabel = TEXT("Back Hair"); break;
	case EUECustomizationPart::HairSide: CatalogCategoryLabel = TEXT("Side Hair"); break;
	case EUECustomizationPart::HairExtra: CatalogCategoryLabel = TEXT("Extensions / Ahoge / Extra"); break;
	case EUECustomizationPart::HairBase: CatalogCategoryLabel = TEXT("Base Hair"); break;
	case EUECustomizationPart::Top: CatalogCategoryLabel = TEXT("Tops"); break;
	case EUECustomizationPart::Bottom: CatalogCategoryLabel = TEXT("Bottoms"); break;
	case EUECustomizationPart::Onepiece: CatalogCategoryLabel = TEXT("Dresses"); break;
	case EUECustomizationPart::Shoes: CatalogCategoryLabel = TEXT("Shoes"); break;
	case EUECustomizationPart::HeadAccessory: CatalogCategoryLabel = TEXT("Head Accessories"); break;
	case EUECustomizationPart::FaceAccessory: CatalogCategoryLabel = TEXT("Face Accessories"); break;
	case EUECustomizationPart::EarAccessory: CatalogCategoryLabel = TEXT("Ear Accessories"); break;
	case EUECustomizationPart::TailAccessory: CatalogCategoryLabel = TEXT("Tail Accessories"); break;
	case EUECustomizationPart::NeckAccessory: CatalogCategoryLabel = TEXT("Neck Accessories"); break;
	default: break;
	}
	CatalogTitle->SetText(FText::FromString(CatalogCategoryLabel));
	CatalogCount->SetText(FText::FromString(FString::Printf(TEXT("%d extracted items"), Count)));
	const int32 SelectedIndex = GetSelectedIndex(CurrentPart);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		USizeBox* TileSize = WidgetTree->ConstructWidget<USizeBox>();
		TileSize->SetWidthOverride(130.0f);
		TileSize->SetHeightOverride(108.0f);
		TileSize->SetClipping(EWidgetClipping::ClipToBounds);
		OptionGrid->AddChildToUniformGrid(TileSize, Index / 3, Index % 3);

		UUECustomizationOptionButton* Button = WidgetTree->ConstructWidget<UUECustomizationOptionButton>();
		Button->Configure(this, CurrentPart, Index);
		Button->SetClipping(EWidgetClipping::ClipToBounds);
		Button->SetBackgroundColor(Index == SelectedIndex ? SelectedColor : FLinearColor(0.10f, 0.12f, 0.14f, 1.0f));
		TileSize->AddChild(Button);
		UVerticalBox* Tile = WidgetTree->ConstructWidget<UVerticalBox>();
		Button->AddChild(Tile);

		if (PreviewActor)
		{
			if (UTexture2D* Texture = PreviewActor->GetOptionTexture(CurrentPart, Index))
			{
				UImage* Thumbnail = WidgetTree->ConstructWidget<UImage>();
				Thumbnail->SetBrushFromTexture(Texture, true);
				Thumbnail->SetDesiredSizeOverride(FVector2D(118.0f, 76.0f));
				Tile->AddChild(Thumbnail);
				CastChecked<UVerticalBoxSlot>(Thumbnail->Slot)->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		UTextBlock* Label = CreateText(
			PreviewActor ? PreviewActor->GetOptionLabel(CurrentPart, Index) : FString::Printf(TEXT("Style %02d"), Index + 1),
			11,
			TextColor);
		Label->SetJustification(ETextJustify::Center);
		Tile->AddChild(Label);
		CastChecked<UVerticalBoxSlot>(Label->Slot)->SetVerticalAlignment(VAlign_Center);
		CastChecked<UVerticalBoxSlot>(Label->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	if (Count == 0)
	{
		UTextBlock* Empty = CreateText(TEXT("No compatible item was extracted for this category."), 13, MutedTextColor);
		Empty->SetAutoWrapText(true);
		OptionGrid->AddChildToUniformGrid(Empty, 0, 0);
	}
}

int32 UUECustomizationWidget::GetSelectedIndex(EUECustomizationPart Part) const
{
	if (!PreviewActor)
	{
		return 0;
	}
	const FUECharacterCustomizationData& Data = PreviewActor->GetAppearance();
	switch (Part)
	{
	case EUECustomizationPart::Gender: return static_cast<int32>(Data.Gender);
	case EUECustomizationPart::Body: return static_cast<int32>(Data.BodyPreset);
	case EUECustomizationPart::FaceSkin: return Data.FaceStyle;
	case EUECustomizationPart::EyeWhite: return Data.EyeWhiteStyle;
	case EUECustomizationPart::EyeIris: return Data.EyeIrisStyle;
	case EUECustomizationPart::EyeHighlight: return Data.EyeHighlightStyle;
	case EUECustomizationPart::EyeExtra: return Data.EyeExtraStyle;
	case EUECustomizationPart::Brow: return Data.BrowStyle;
	case EUECustomizationPart::Eyelash: return Data.EyelashStyle;
	case EUECustomizationPart::Eyeline: return Data.EyelineStyle;
	case EUECustomizationPart::Mouth: return Data.MouthStyle;
	case EUECustomizationPart::Lip: return Data.LipStyle;
	case EUECustomizationPart::MouthLine: return Data.MouthLineStyle;
	case EUECustomizationPart::HairSet:
	case EUECustomizationPart::HairFront: return Data.HairFrontStyle;
	case EUECustomizationPart::HairSide: return Data.HairSideStyle;
	case EUECustomizationPart::HairBack: return Data.HairBackStyle;
	case EUECustomizationPart::HairExtra: return Data.HairExtraStyle;
	case EUECustomizationPart::HairBase: return Data.HairBaseStyle;
	case EUECustomizationPart::Top: return Data.TopStyle;
	case EUECustomizationPart::Bottom: return Data.BottomStyle;
	case EUECustomizationPart::Onepiece: return Data.OnepieceStyle;
	case EUECustomizationPart::Shoes: return Data.ShoesStyle;
	case EUECustomizationPart::HeadAccessory: return Data.HeadAccessoryStyle;
	case EUECustomizationPart::FaceAccessory: return Data.FaceAccessoryStyle;
	case EUECustomizationPart::EarAccessory: return Data.EarAccessoryStyle;
	case EUECustomizationPart::TailAccessory: return Data.TailAccessoryStyle;
	case EUECustomizationPart::NeckAccessory: return Data.NeckAccessoryStyle;
	}
	return 0;
}

void UUECustomizationWidget::SelectPartOption(EUECustomizationPart Part, int32 Index)
{
	if (!PreviewActor || !FMath::IsWithin(Index, 0, PreviewActor->GetOptionCount(Part)))
	{
		return;
	}
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	switch (Part)
	{
	case EUECustomizationPart::Gender: Data.Gender = static_cast<EUECharacterGender>(Index); break;
	case EUECustomizationPart::Body: Data.BodyPreset = static_cast<EUEBodyPreset>(Index); break;
	case EUECustomizationPart::FaceSkin: Data.FaceStyle = Index; break;
	case EUECustomizationPart::EyeWhite: Data.EyeWhiteStyle = Index; break;
	case EUECustomizationPart::EyeIris: Data.EyeIrisStyle = Index; break;
	case EUECustomizationPart::EyeHighlight: Data.EyeHighlightStyle = Index; break;
	case EUECustomizationPart::EyeExtra: Data.EyeExtraStyle = Index; break;
	case EUECustomizationPart::Brow: Data.BrowStyle = Index; break;
	case EUECustomizationPart::Eyelash: Data.EyelashStyle = Index; break;
	case EUECustomizationPart::Eyeline: Data.EyelineStyle = Index; break;
	case EUECustomizationPart::Mouth: Data.MouthStyle = Index; break;
	case EUECustomizationPart::Lip: Data.LipStyle = Index; break;
	case EUECustomizationPart::MouthLine: Data.MouthLineStyle = Index; break;
	case EUECustomizationPart::HairSet:
		Data.HairFrontStyle = Index;
		Data.HairSideStyle = Index;
		Data.HairBackStyle = Index;
		Data.HairExtraStyle = Index;
		break;
	case EUECustomizationPart::HairFront: Data.HairFrontStyle = Index; break;
	case EUECustomizationPart::HairSide: Data.HairSideStyle = Index; break;
	case EUECustomizationPart::HairBack: Data.HairBackStyle = Index; break;
	case EUECustomizationPart::HairExtra: Data.HairExtraStyle = Index; break;
	case EUECustomizationPart::HairBase: Data.HairBaseStyle = Index; break;
	case EUECustomizationPart::Top:
		Data.TopStyle = Index;
		if (Index > 0) Data.OnepieceStyle = 0;
		break;
	case EUECustomizationPart::Bottom:
		Data.BottomStyle = Index;
		if (Index > 0) Data.OnepieceStyle = 0;
		break;
	case EUECustomizationPart::Onepiece:
		Data.OnepieceStyle = Index;
		if (Index > 0)
		{
			Data.TopStyle = 0;
			Data.BottomStyle = 0;
		}
		break;
	case EUECustomizationPart::Shoes: Data.ShoesStyle = Index; break;
	case EUECustomizationPart::HeadAccessory: Data.HeadAccessoryStyle = Index; break;
	case EUECustomizationPart::FaceAccessory: Data.FaceAccessoryStyle = Index; break;
	case EUECustomizationPart::EarAccessory: Data.EarAccessoryStyle = Index; break;
	case EUECustomizationPart::TailAccessory: Data.TailAccessoryStyle = Index; break;
	case EUECustomizationPart::NeckAccessory: Data.NeckAccessoryStyle = Index; break;
	}
	PreviewActor->ApplyAppearance(Data);
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(
			TEXT("Applied %s immediately"), *PreviewActor->GetOptionLabel(Part, Index))));
	}
	RebuildCatalog();
}

void UUECustomizationWidget::SynchronizeControls()
{
	if (!PreviewActor || !HeightSlider || !HeadSizeSlider || !ShoulderSlider)
	{
		return;
	}
	bSynchronizingControls = true;
	const FUECharacterCustomizationData& Data = PreviewActor->GetAppearance();
	HeightSlider->SetValue(Data.Height);
	HeadSizeSlider->SetValue(Data.HeadSize);
	ShoulderSlider->SetValue(Data.ShoulderWidth);
	bSynchronizingControls = false;
	RebuildCategories();
	RebuildCatalog();
}

void UUECustomizationWidget::ShowBody() { SetSection(EEditorSection::Body); }
void UUECustomizationWidget::ShowFace() { SetSection(EEditorSection::Face); }
void UUECustomizationWidget::ShowHairstyle() { SetSection(EEditorSection::Hairstyle); }
void UUECustomizationWidget::ShowOutfit() { SetSection(EEditorSection::Outfit); }
void UUECustomizationWidget::ShowAccessory() { SetSection(EEditorSection::Accessory); }

void UUECustomizationWidget::HeightChanged(float Value)
{
	if (!PreviewActor || bSynchronizingControls) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	Data.Height = Value;
	PreviewActor->ApplyAppearance(Data);
}

void UUECustomizationWidget::HeadSizeChanged(float Value)
{
	if (!PreviewActor || bSynchronizingControls) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	Data.HeadSize = Value;
	PreviewActor->ApplyAppearance(Data);
}

void UUECustomizationWidget::ShoulderWidthChanged(float Value)
{
	if (!PreviewActor || bSynchronizingControls) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	Data.ShoulderWidth = Value;
	PreviewActor->ApplyAppearance(Data);
}

void UUECustomizationWidget::RotateLeft() { if (PreviewActor) PreviewActor->RotatePreview(-15.0f); }
void UUECustomizationWidget::RotateRight() { if (PreviewActor) PreviewActor->RotatePreview(15.0f); }

void UUECustomizationWidget::Save()
{
	if (StatusText) StatusText->SetText(FText::FromString(PreviewActor && PreviewActor->SaveAppearance() ? TEXT("Saved") : TEXT("Save failed")));
}

void UUECustomizationWidget::Reset()
{
	if (!PreviewActor) return;
	PreviewActor->ResetAppearance();
	SynchronizeControls();
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("Reset to defaults")));
}

void UUECustomizationWidget::Randomize()
{
	if (!PreviewActor) return;
	PreviewActor->RandomizeAppearance();
	SynchronizeControls();
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("Randomized from extracted presets")));
}

#define UE_SET_LINEAR_COLOR(FunctionName, FieldName, ColorValue) \
	void UUECustomizationWidget::FunctionName() \
	{ \
		if (!PreviewActor) return; \
		FUECharacterCustomizationData Data = PreviewActor->GetAppearance(); \
		Data.FieldName = ColorValue; \
		PreviewActor->ApplyAppearance(Data); \
	}

UE_SET_LINEAR_COLOR(SkinLight, SkinColor, FLinearColor::FromSRGBColor(FColor(239, 185, 151)))
UE_SET_LINEAR_COLOR(SkinMedium, SkinColor, FLinearColor::FromSRGBColor(FColor(184, 128, 92)))
UE_SET_LINEAR_COLOR(SkinDeep, SkinColor, FLinearColor::FromSRGBColor(FColor(105, 66, 47)))
UE_SET_LINEAR_COLOR(HairOriginal, HairColor, FLinearColor::White)
UE_SET_LINEAR_COLOR(HairBlue, HairColor, FLinearColor(0.12f, 0.22f, 0.55f))
UE_SET_LINEAR_COLOR(HairCoral, HairColor, FLinearColor(0.55f, 0.12f, 0.16f))
UE_SET_LINEAR_COLOR(EyeOriginal, EyeColor, FLinearColor(0.38f, 0.16f, 0.06f))
UE_SET_LINEAR_COLOR(EyeBlue, EyeColor, FLinearColor(0.12f, 0.38f, 0.72f))
UE_SET_LINEAR_COLOR(EyeBrown, EyeColor, FLinearColor(0.16f, 0.42f, 0.22f))
UE_SET_LINEAR_COLOR(LipNatural, LipColor, FLinearColor::FromSRGBColor(FColor(196, 102, 116)))
UE_SET_LINEAR_COLOR(LipSoft, LipColor, FLinearColor::FromSRGBColor(FColor(225, 154, 165)))
UE_SET_LINEAR_COLOR(LipDeep, LipColor, FLinearColor::FromSRGBColor(FColor(133, 52, 70)))
UE_SET_LINEAR_COLOR(OutfitOriginal, OutfitColor, FLinearColor::White)
UE_SET_LINEAR_COLOR(OutfitCyan, OutfitColor, FLinearColor(0.25f, 0.65f, 1.0f))
UE_SET_LINEAR_COLOR(OutfitRed, OutfitColor, FLinearColor(1.0f, 0.20f, 0.16f))

#undef UE_SET_LINEAR_COLOR
