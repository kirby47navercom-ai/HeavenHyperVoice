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
#include "Components/SpinBox.h"
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

void UUECustomizationColorButton::Configure(
	UUECustomizationWidget* InOwner,
	EUECustomizationColorChannel InChannel,
	const FLinearColor& InColor)
{
	OwnerWidget = InOwner;
	Channel = InChannel;
	Color = InColor;
	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UUECustomizationColorButton::HandleClicked()
{
	if (OwnerWidget)
	{
		OwnerWidget->SelectColor(Channel, Color);
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
	const TGuardValue<bool> SynchronizeGuard(bSynchronizingControls, true);

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
	UScrollBox* ParameterScroll = WidgetTree->ConstructWidget<UScrollBox>();
	ParameterScroll->SetAlwaysShowScrollbar(true);
	ParameterPanel->AddChild(ParameterScroll);
	UVerticalBox* Parameters = WidgetTree->ConstructWidget<UVerticalBox>();
	ParameterScroll->AddChild(Parameters);

	AddSectionTitle(Parameters, TEXT("PARAMETERS"));
	AddSliderRow(Parameters, TEXT("Height"), HeightSlider);
	AddSliderRow(Parameters, TEXT("Head Size"), HeadSizeSlider);
	AddSliderRow(Parameters, TEXT("Shoulder / Body Width"), ShoulderSlider);
	HeightSlider->OnValueChanged.AddDynamic(this, &ThisClass::HeightChanged);
	HeadSizeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HeadSizeChanged);
	ShoulderSlider->OnValueChanged.AddDynamic(this, &ThisClass::ShoulderWidthChanged);

	TArray<UButton*> SkinButtons;
	AddColorRow(Parameters, TEXT("Skin color"), EUECustomizationColorChannel::Skin, {
		FLinearColor::FromSRGBColor(FColor(255, 224, 196)),
		FLinearColor::FromSRGBColor(FColor(239, 185, 151)),
		FLinearColor::FromSRGBColor(FColor(220, 154, 115)),
		FLinearColor::FromSRGBColor(FColor(184, 128, 92)),
		FLinearColor::FromSRGBColor(FColor(152, 98, 67)),
		FLinearColor::FromSRGBColor(FColor(105, 66, 47)),
		FLinearColor::FromSRGBColor(FColor(255, 196, 170)),
		FLinearColor::FromSRGBColor(FColor(242, 170, 143)),
		FLinearColor::FromSRGBColor(FColor(197, 143, 116)),
		FLinearColor::FromSRGBColor(FColor(139, 91, 70)),
		FLinearColor::FromSRGBColor(FColor(92, 55, 44)),
		FLinearColor::FromSRGBColor(FColor(66, 42, 38))}, SkinButtons);
	AddRGBColorRow(Parameters, TEXT("Skin RGB 0-255"), SkinRedInput, SkinGreenInput, SkinBlueInput);

	TArray<UButton*> EyeButtons;
	AddColorRow(Parameters, TEXT("Iris color"), EUECustomizationColorChannel::Eye, {
		FLinearColor::FromSRGBColor(FColor(28, 22, 18)),
		FLinearColor::FromSRGBColor(FColor(92, 51, 25)),
		FLinearColor::FromSRGBColor(FColor(145, 85, 38)),
		FLinearColor::FromSRGBColor(FColor(201, 140, 54)),
		FLinearColor::FromSRGBColor(FColor(70, 105, 145)),
		FLinearColor::FromSRGBColor(FColor(45, 150, 210)),
		FLinearColor::FromSRGBColor(FColor(45, 155, 95)),
		FLinearColor::FromSRGBColor(FColor(127, 65, 165)),
		FLinearColor::FromSRGBColor(FColor(190, 65, 115)),
		FLinearColor::FromSRGBColor(FColor(190, 45, 48)),
		FLinearColor::FromSRGBColor(FColor(230, 220, 165)),
		FLinearColor::FromSRGBColor(FColor(190, 195, 205))}, EyeButtons);
	AddRGBColorRow(Parameters, TEXT("Iris RGB 0-255"), EyeRedInput, EyeGreenInput, EyeBlueInput);

	TArray<UButton*> LipButtons;
	AddColorRow(Parameters, TEXT("Lip color"), EUECustomizationColorChannel::Lip, {
		FLinearColor::FromSRGBColor(FColor(105, 34, 48)),
		FLinearColor::FromSRGBColor(FColor(133, 52, 70)),
		FLinearColor::FromSRGBColor(FColor(166, 70, 89)),
		FLinearColor::FromSRGBColor(FColor(196, 102, 116)),
		FLinearColor::FromSRGBColor(FColor(225, 154, 165)),
		FLinearColor::FromSRGBColor(FColor(244, 190, 193)),
		FLinearColor::FromSRGBColor(FColor(145, 49, 110)),
		FLinearColor::FromSRGBColor(FColor(191, 62, 135)),
		FLinearColor::FromSRGBColor(FColor(103, 42, 88)),
		FLinearColor::FromSRGBColor(FColor(218, 88, 75)),
		FLinearColor::FromSRGBColor(FColor(239, 119, 100)),
		FLinearColor::FromSRGBColor(FColor(92, 38, 38))}, LipButtons);
	AddRGBColorRow(Parameters, TEXT("Lip RGB 0-255"), LipRedInput, LipGreenInput, LipBlueInput);

	TArray<UButton*> HairButtons;
	AddColorRow(Parameters, TEXT("Hair color"), EUECustomizationColorChannel::Hair, {
		FLinearColor::FromSRGBColor(FColor(28, 15, 12)),
		FLinearColor::FromSRGBColor(FColor(67, 28, 17)),
		FLinearColor::FromSRGBColor(FColor(108, 48, 23)),
		FLinearColor::FromSRGBColor(FColor(155, 76, 35)),
		FLinearColor::FromSRGBColor(FColor(212, 133, 78)),
		FLinearColor::FromSRGBColor(FColor(235, 188, 112)),
		FLinearColor::FromSRGBColor(FColor(28, 35, 72)),
		FLinearColor::FromSRGBColor(FColor(52, 92, 180)),
		FLinearColor::FromSRGBColor(FColor(112, 57, 155)),
		FLinearColor::FromSRGBColor(FColor(186, 56, 116)),
		FLinearColor::FromSRGBColor(FColor(45, 145, 103)),
		FLinearColor::FromSRGBColor(FColor(222, 205, 69))}, HairButtons);
	AddRGBColorRow(Parameters, TEXT("Hair RGB 0-255"), HairRedInput, HairGreenInput, HairBlueInput);

	const TArray<FLinearColor> OutfitPalette = {
		FLinearColor::FromSRGBColor(FColor(245, 245, 245)),
		FLinearColor::FromSRGBColor(FColor(28, 50, 80)),
		FLinearColor::FromSRGBColor(FColor(54, 104, 160)),
		FLinearColor::FromSRGBColor(FColor(61, 164, 190)),
		FLinearColor::FromSRGBColor(FColor(51, 145, 110)),
		FLinearColor::FromSRGBColor(FColor(120, 158, 64)),
		FLinearColor::FromSRGBColor(FColor(231, 193, 65)),
		FLinearColor::FromSRGBColor(FColor(230, 134, 58)),
		FLinearColor::FromSRGBColor(FColor(190, 63, 52)),
		FLinearColor::FromSRGBColor(FColor(158, 55, 117)),
		FLinearColor::FromSRGBColor(FColor(102, 69, 156)),
		FLinearColor::FromSRGBColor(FColor(70, 70, 78))};
	TArray<UButton*> OutfitButtons;
	AddColorRow(Parameters, TEXT("All outfit color"), EUECustomizationColorChannel::Outfit, OutfitPalette, OutfitButtons);
	AddRGBColorRow(Parameters, TEXT("Outfit RGB 0-255"), OutfitRedInput, OutfitGreenInput, OutfitBlueInput);

	TArray<UButton*> TopButtons;
	AddColorRow(Parameters, TEXT("Top color"), EUECustomizationColorChannel::Top, OutfitPalette, TopButtons);
	AddRGBColorRow(Parameters, TEXT("Top RGB 0-255"), TopRedInput, TopGreenInput, TopBlueInput);

	TArray<UButton*> BottomButtons;
	AddColorRow(Parameters, TEXT("Bottom color"), EUECustomizationColorChannel::Bottom, OutfitPalette, BottomButtons);
	AddRGBColorRow(Parameters, TEXT("Bottom RGB 0-255"), BottomRedInput, BottomGreenInput, BottomBlueInput);

	TArray<UButton*> OnepieceButtons;
	AddColorRow(Parameters, TEXT("Onepiece color"), EUECustomizationColorChannel::Onepiece, OutfitPalette, OnepieceButtons);
	AddRGBColorRow(Parameters, TEXT("Onepiece RGB 0-255"), OnepieceRedInput, OnepieceGreenInput, OnepieceBlueInput);

	TArray<UButton*> ShoesButtons;
	AddColorRow(Parameters, TEXT("Shoes color"), EUECustomizationColorChannel::Shoes, OutfitPalette, ShoesButtons);
	AddRGBColorRow(Parameters, TEXT("Shoes RGB 0-255"), ShoesRedInput, ShoesGreenInput, ShoesBlueInput);

	TArray<UButton*> AccessoryButtons;
	AddColorRow(Parameters, TEXT("Accessory color"), EUECustomizationColorChannel::Accessory, OutfitPalette, AccessoryButtons);
	AddRGBColorRow(Parameters, TEXT("Accessory RGB 0-255"), AccessoryRedInput, AccessoryGreenInput, AccessoryBlueInput);

	SkinRedInput->OnValueChanged.AddDynamic(this, &ThisClass::SkinRedChanged);
	SkinGreenInput->OnValueChanged.AddDynamic(this, &ThisClass::SkinGreenChanged);
	SkinBlueInput->OnValueChanged.AddDynamic(this, &ThisClass::SkinBlueChanged);
	HairRedInput->OnValueChanged.AddDynamic(this, &ThisClass::HairRedChanged);
	HairGreenInput->OnValueChanged.AddDynamic(this, &ThisClass::HairGreenChanged);
	HairBlueInput->OnValueChanged.AddDynamic(this, &ThisClass::HairBlueChanged);
	EyeRedInput->OnValueChanged.AddDynamic(this, &ThisClass::EyeRedChanged);
	EyeGreenInput->OnValueChanged.AddDynamic(this, &ThisClass::EyeGreenChanged);
	EyeBlueInput->OnValueChanged.AddDynamic(this, &ThisClass::EyeBlueChanged);
	LipRedInput->OnValueChanged.AddDynamic(this, &ThisClass::LipRedChanged);
	LipGreenInput->OnValueChanged.AddDynamic(this, &ThisClass::LipGreenChanged);
	LipBlueInput->OnValueChanged.AddDynamic(this, &ThisClass::LipBlueChanged);
	OutfitRedInput->OnValueChanged.AddDynamic(this, &ThisClass::OutfitRedChanged);
	OutfitGreenInput->OnValueChanged.AddDynamic(this, &ThisClass::OutfitGreenChanged);
	OutfitBlueInput->OnValueChanged.AddDynamic(this, &ThisClass::OutfitBlueChanged);
	TopRedInput->OnValueChanged.AddDynamic(this, &ThisClass::TopRedChanged);
	TopGreenInput->OnValueChanged.AddDynamic(this, &ThisClass::TopGreenChanged);
	TopBlueInput->OnValueChanged.AddDynamic(this, &ThisClass::TopBlueChanged);
	BottomRedInput->OnValueChanged.AddDynamic(this, &ThisClass::BottomRedChanged);
	BottomGreenInput->OnValueChanged.AddDynamic(this, &ThisClass::BottomGreenChanged);
	BottomBlueInput->OnValueChanged.AddDynamic(this, &ThisClass::BottomBlueChanged);
	OnepieceRedInput->OnValueChanged.AddDynamic(this, &ThisClass::OnepieceRedChanged);
	OnepieceGreenInput->OnValueChanged.AddDynamic(this, &ThisClass::OnepieceGreenChanged);
	OnepieceBlueInput->OnValueChanged.AddDynamic(this, &ThisClass::OnepieceBlueChanged);
	ShoesRedInput->OnValueChanged.AddDynamic(this, &ThisClass::ShoesRedChanged);
	ShoesGreenInput->OnValueChanged.AddDynamic(this, &ThisClass::ShoesGreenChanged);
	ShoesBlueInput->OnValueChanged.AddDynamic(this, &ThisClass::ShoesBlueChanged);
	AccessoryRedInput->OnValueChanged.AddDynamic(this, &ThisClass::AccessoryRedChanged);
	AccessoryGreenInput->OnValueChanged.AddDynamic(this, &ThisClass::AccessoryGreenChanged);
	AccessoryBlueInput->OnValueChanged.AddDynamic(this, &ThisClass::AccessoryBlueChanged);

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
	EUECustomizationColorChannel Channel,
	const TArray<FLinearColor>& Colors,
	TArray<UButton*>& OutButtons)
{
	Parent->AddChild(CreateText(Label, 12, MutedTextColor));
	UUniformGridPanel* Row = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Row->SetSlotPadding(FMargin(2.0f));
	Parent->AddChild(Row);
	for (int32 Index = 0; Index < Colors.Num(); ++Index)
	{
		const FLinearColor& Color = Colors[Index];
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(38.0f);
		Size->SetHeightOverride(26.0f);
		Row->AddChildToUniformGrid(Size, Index / 6, Index % 6);
		UUECustomizationColorButton* Button = WidgetTree->ConstructWidget<UUECustomizationColorButton>();
		Button->Configure(this, Channel, Color);
		Button->SetBackgroundColor(Color);
		Size->AddChild(Button);
		OutButtons.Add(Button);
	}
}

void UUECustomizationWidget::AddRGBColorRow(
	UVerticalBox* Parent,
	const FString& Label,
	TObjectPtr<USpinBox>& OutRed,
	TObjectPtr<USpinBox>& OutGreen,
	TObjectPtr<USpinBox>& OutBlue)
{
	Parent->AddChild(CreateText(Label, 11, MutedTextColor));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChild(Row);

	const TCHAR* ChannelLabels[] = {TEXT("R"), TEXT("G"), TEXT("B")};
	TObjectPtr<USpinBox>* Outputs[] = {&OutRed, &OutGreen, &OutBlue};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UHorizontalBox* ChannelRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		Row->AddChild(ChannelRow);
		CastChecked<UHorizontalBoxSlot>(ChannelRow->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CastChecked<UHorizontalBoxSlot>(ChannelRow->Slot)->SetPadding(FMargin(1.0f));

		UTextBlock* ChannelLabel = CreateText(ChannelLabels[Index], 11, TextColor);
		ChannelRow->AddChild(ChannelLabel);
		CastChecked<UHorizontalBoxSlot>(ChannelLabel->Slot)->SetVerticalAlignment(VAlign_Center);
		CastChecked<UHorizontalBoxSlot>(ChannelLabel->Slot)->SetPadding(FMargin(0.0f, 0.0f, 2.0f, 0.0f));

		USpinBox* Input = WidgetTree->ConstructWidget<USpinBox>();
		Input->SetMinValue(0.0f);
		Input->SetMaxValue(255.0f);
		Input->SetDelta(1.0f);
		Input->SetMinFractionalDigits(0);
		Input->SetMaxFractionalDigits(0);
		Input->SetValue(255.0f);
		Input->SetForegroundColor(TextColor);
		ChannelRow->AddChild(Input);
		*Outputs[Index] = Input;
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
		const FString OptionLabel = PreviewActor
			? PreviewActor->GetOptionLabel(CurrentPart, Index)
			: FString::Printf(TEXT("Style %02d"), Index + 1);
		USizeBox* TileSize = WidgetTree->ConstructWidget<USizeBox>();
		TileSize->SetWidthOverride(122.0f);
		TileSize->SetHeightOverride(144.0f);
		TileSize->SetClipping(EWidgetClipping::ClipToBounds);
		OptionGrid->AddChildToUniformGrid(TileSize, Index / 3, Index % 3);

		UUECustomizationOptionButton* Button = WidgetTree->ConstructWidget<UUECustomizationOptionButton>();
		Button->Configure(this, CurrentPart, Index);
		Button->SetClipping(EWidgetClipping::ClipToBounds);
		Button->SetBackgroundColor(Index == SelectedIndex ? SelectedColor : FLinearColor(0.10f, 0.12f, 0.14f, 1.0f));
		TileSize->AddChild(Button);
		UVerticalBox* Tile = WidgetTree->ConstructWidget<UVerticalBox>();
		Button->AddChild(Tile);

		USizeBox* ThumbnailSize = WidgetTree->ConstructWidget<USizeBox>();
		ThumbnailSize->SetWidthOverride(114.0f);
		ThumbnailSize->SetHeightOverride(92.0f);
		UBorder* ThumbnailFrame = WidgetTree->ConstructWidget<UBorder>();
		ThumbnailFrame->SetBrushColor(FLinearColor(0.035f, 0.045f, 0.055f, 1.0f));
		ThumbnailFrame->SetPadding(FMargin(3.0f));
		ThumbnailSize->AddChild(ThumbnailFrame);
		Tile->AddChild(ThumbnailSize);

		if (PreviewActor)
		{
			if (UTexture2D* Texture = PreviewActor->GetOptionTexture(CurrentPart, Index))
			{
				UImage* Thumbnail = WidgetTree->ConstructWidget<UImage>();
				Thumbnail->SetBrushFromTexture(Texture, true);
				Thumbnail->SetColorAndOpacity(FLinearColor::White);
				ThumbnailFrame->AddChild(Thumbnail);
			}
			else
			{
				UTextBlock* MeshHint = CreateText(OptionLabel, 10, MutedTextColor);
				MeshHint->SetJustification(ETextJustify::Center);
				MeshHint->SetAutoWrapText(true);
				ThumbnailFrame->AddChild(MeshHint);
			}
		}
		UTextBlock* Label = CreateText(
			FString::Printf(
				TEXT("#%02d  %s"),
				Index + 1,
				*OptionLabel),
			11,
			TextColor);
		Label->SetJustification(ETextJustify::Center);
		Label->SetAutoWrapText(true);
		Tile->AddChild(Label);
		CastChecked<UVerticalBoxSlot>(Label->Slot)->SetVerticalAlignment(VAlign_Center);
		CastChecked<UVerticalBoxSlot>(Label->Slot)->SetPadding(FMargin(2.0f, 4.0f, 2.0f, 0.0f));
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
		Data.HairBaseStyle = 1;
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
	SynchronizeColorControls();
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

FColor UUECustomizationWidget::ToSRGB8(const FLinearColor& Color)
{
	return Color.ToFColorSRGB();
}

void UUECustomizationWidget::SynchronizeColorControls()
{
	if (!PreviewActor)
	{
		return;
	}
	const FUECharacterCustomizationData& Data = PreviewActor->GetAppearance();
	const FColor Skin = ToSRGB8(Data.SkinColor);
	const FColor Hair = ToSRGB8(Data.HairColor);
	const FColor Eye = ToSRGB8(Data.EyeColor);
	const FColor Lip = ToSRGB8(Data.LipColor);
	const FColor Outfit = ToSRGB8(Data.OutfitColor);
	const FColor Top = ToSRGB8(Data.TopColor);
	const FColor Bottom = ToSRGB8(Data.BottomColor);
	const FColor Onepiece = ToSRGB8(Data.OnepieceColor);
	const FColor Shoes = ToSRGB8(Data.ShoesColor);
	const FColor Accessory = ToSRGB8(Data.AccessoryColor);

	const auto SetInputs = [](const FColor& Color, USpinBox* Red, USpinBox* Green, USpinBox* Blue)
	{
		if (Red) Red->SetValue(static_cast<float>(Color.R));
		if (Green) Green->SetValue(static_cast<float>(Color.G));
		if (Blue) Blue->SetValue(static_cast<float>(Color.B));
	};
	SetInputs(Skin, SkinRedInput, SkinGreenInput, SkinBlueInput);
	SetInputs(Hair, HairRedInput, HairGreenInput, HairBlueInput);
	SetInputs(Eye, EyeRedInput, EyeGreenInput, EyeBlueInput);
	SetInputs(Lip, LipRedInput, LipGreenInput, LipBlueInput);
	SetInputs(Outfit, OutfitRedInput, OutfitGreenInput, OutfitBlueInput);
	SetInputs(Top, TopRedInput, TopGreenInput, TopBlueInput);
	SetInputs(Bottom, BottomRedInput, BottomGreenInput, BottomBlueInput);
	SetInputs(Onepiece, OnepieceRedInput, OnepieceGreenInput, OnepieceBlueInput);
	SetInputs(Shoes, ShoesRedInput, ShoesGreenInput, ShoesBlueInput);
	SetInputs(Accessory, AccessoryRedInput, AccessoryGreenInput, AccessoryBlueInput);
}

void UUECustomizationWidget::SelectColor(
	EUECustomizationColorChannel Channel,
	const FLinearColor& Color)
{
	if (!PreviewActor)
	{
		return;
	}
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	auto ApplyWholeOutfitColor = [&Data](const FLinearColor& NewColor)
	{
		Data.OutfitColor = NewColor;
		Data.TopColor = NewColor;
		Data.BottomColor = NewColor;
		Data.OnepieceColor = NewColor;
		Data.ShoesColor = NewColor;
		Data.AccessoryColor = NewColor;
	};
	switch (Channel)
	{
	case EUECustomizationColorChannel::Skin: Data.SkinColor = Color; break;
	case EUECustomizationColorChannel::Hair: Data.HairColor = Color; break;
	case EUECustomizationColorChannel::Eye: Data.EyeColor = Color; break;
	case EUECustomizationColorChannel::Lip: Data.LipColor = Color; break;
	case EUECustomizationColorChannel::Outfit: ApplyWholeOutfitColor(Color); break;
	case EUECustomizationColorChannel::Top: Data.TopColor = Color; break;
	case EUECustomizationColorChannel::Bottom: Data.BottomColor = Color; break;
	case EUECustomizationColorChannel::Onepiece: Data.OnepieceColor = Color; break;
	case EUECustomizationColorChannel::Shoes: Data.ShoesColor = Color; break;
	case EUECustomizationColorChannel::Accessory: Data.AccessoryColor = Color; break;
	}
	PreviewActor->ApplyAppearance(Data);
	bSynchronizingControls = true;
	SynchronizeColorControls();
	bSynchronizingControls = false;
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(TEXT("Color applied immediately")));
	}
}

void UUECustomizationWidget::UpdateColorChannel(
	EUECustomizationColorChannel Channel,
	int32 Component,
	float Value)
{
	if (!PreviewActor || bSynchronizingControls)
	{
		return;
	}
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	auto ApplyWholeOutfitColor = [&Data](const FLinearColor& NewColor)
	{
		Data.OutfitColor = NewColor;
		Data.TopColor = NewColor;
		Data.BottomColor = NewColor;
		Data.OnepieceColor = NewColor;
		Data.ShoesColor = NewColor;
		Data.AccessoryColor = NewColor;
	};
	FLinearColor* Target = nullptr;
	switch (Channel)
	{
	case EUECustomizationColorChannel::Skin: Target = &Data.SkinColor; break;
	case EUECustomizationColorChannel::Hair: Target = &Data.HairColor; break;
	case EUECustomizationColorChannel::Eye: Target = &Data.EyeColor; break;
	case EUECustomizationColorChannel::Lip: Target = &Data.LipColor; break;
	case EUECustomizationColorChannel::Outfit: Target = &Data.OutfitColor; break;
	case EUECustomizationColorChannel::Top: Target = &Data.TopColor; break;
	case EUECustomizationColorChannel::Bottom: Target = &Data.BottomColor; break;
	case EUECustomizationColorChannel::Onepiece: Target = &Data.OnepieceColor; break;
	case EUECustomizationColorChannel::Shoes: Target = &Data.ShoesColor; break;
	case EUECustomizationColorChannel::Accessory: Target = &Data.AccessoryColor; break;
	}
	if (!Target)
	{
		return;
	}
	FColor Current = ToSRGB8(*Target);
	uint8* ChannelValue = Component == 0
		? &Current.R
		: Component == 1
			? &Current.G
			: &Current.B;
	const uint8 NewValue = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Value), 0, 255));
	if (*ChannelValue == NewValue)
	{
		return;
	}
	*ChannelValue = NewValue;
	*Target = FLinearColor::FromSRGBColor(Current);
	if (Channel == EUECustomizationColorChannel::Outfit)
	{
		ApplyWholeOutfitColor(*Target);
	}
	PreviewActor->ApplyAppearance(Data);
	if (Channel == EUECustomizationColorChannel::Outfit)
	{
		bSynchronizingControls = true;
		SynchronizeColorControls();
		bSynchronizingControls = false;
	}
}

void UUECustomizationWidget::SkinRedChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Skin, 0, Value); }
void UUECustomizationWidget::SkinGreenChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Skin, 1, Value); }
void UUECustomizationWidget::SkinBlueChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Skin, 2, Value); }
void UUECustomizationWidget::HairRedChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Hair, 0, Value); }
void UUECustomizationWidget::HairGreenChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Hair, 1, Value); }
void UUECustomizationWidget::HairBlueChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Hair, 2, Value); }
void UUECustomizationWidget::EyeRedChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Eye, 0, Value); }
void UUECustomizationWidget::EyeGreenChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Eye, 1, Value); }
void UUECustomizationWidget::EyeBlueChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Eye, 2, Value); }
void UUECustomizationWidget::LipRedChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Lip, 0, Value); }
void UUECustomizationWidget::LipGreenChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Lip, 1, Value); }
void UUECustomizationWidget::LipBlueChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Lip, 2, Value); }
void UUECustomizationWidget::OutfitRedChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Outfit, 0, Value); }
void UUECustomizationWidget::OutfitGreenChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Outfit, 1, Value); }
void UUECustomizationWidget::OutfitBlueChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Outfit, 2, Value); }
void UUECustomizationWidget::TopRedChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Top, 0, Value); }
void UUECustomizationWidget::TopGreenChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Top, 1, Value); }
void UUECustomizationWidget::TopBlueChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Top, 2, Value); }
void UUECustomizationWidget::BottomRedChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Bottom, 0, Value); }
void UUECustomizationWidget::BottomGreenChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Bottom, 1, Value); }
void UUECustomizationWidget::BottomBlueChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Bottom, 2, Value); }
void UUECustomizationWidget::OnepieceRedChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Onepiece, 0, Value); }
void UUECustomizationWidget::OnepieceGreenChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Onepiece, 1, Value); }
void UUECustomizationWidget::OnepieceBlueChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Onepiece, 2, Value); }
void UUECustomizationWidget::ShoesRedChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Shoes, 0, Value); }
void UUECustomizationWidget::ShoesGreenChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Shoes, 1, Value); }
void UUECustomizationWidget::ShoesBlueChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Shoes, 2, Value); }
void UUECustomizationWidget::AccessoryRedChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Accessory, 0, Value); }
void UUECustomizationWidget::AccessoryGreenChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Accessory, 1, Value); }
void UUECustomizationWidget::AccessoryBlueChanged(float Value) { UpdateColorChannel(EUECustomizationColorChannel::Accessory, 2, Value); }

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
