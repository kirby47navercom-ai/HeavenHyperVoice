#include "UECustomizationWidget.h"

#include "../Preview/UECustomizationPreviewActor.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor PanelColor(0.015f, 0.035f, 0.065f, 0.94f);
	const FLinearColor TextColor(0.93f, 0.97f, 1.0f, 1.0f);
	const FLinearColor MutedTextColor(0.56f, 0.68f, 0.80f, 1.0f);
	const FLinearColor CyanColor(0.02f, 0.68f, 1.0f, 1.0f);
	const FLinearColor YellowColor(1.0f, 0.78f, 0.04f, 1.0f);
}

void UUECustomizationWidget::SetPreviewActor(AUECustomizationPreviewActor* InPreviewActor)
{
	PreviewActor = InPreviewActor;
	SynchronizeControls();
}

TSharedRef<SWidget> UUECustomizationWidget::RebuildWidget()
{
	// Native UMG classes have no designer tree, so build it before Slate asks for a root widget.
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

	UTextBlock* Title = CreateText(TEXT("CHARACTER CUSTOMIZATION"), 34, TextColor);
	Root->AddChild(Title);
	if (UCanvasPanelSlot* TitleSlot = Cast<UCanvasPanelSlot>(Title->Slot))
	{
		TitleSlot->SetPosition(FVector2D(32.0f, 24.0f));
		TitleSlot->SetSize(FVector2D(580.0f, 52.0f));
	}

	UTextBlock* Subtitle = CreateText(TEXT("Create a profile that can later drive modular skeletal meshes."), 15, MutedTextColor);
	Root->AddChild(Subtitle);
	if (UCanvasPanelSlot* SubtitleSlot = Cast<UCanvasPanelSlot>(Subtitle->Slot))
	{
		SubtitleSlot->SetPosition(FVector2D(34.0f, 70.0f));
		SubtitleSlot->SetSize(FVector2D(720.0f, 30.0f));
	}

	UHorizontalBox* MainLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MainLayout"));
	Root->AddChild(MainLayout);
	if (UCanvasPanelSlot* MainSlot = Cast<UCanvasPanelSlot>(MainLayout->Slot))
	{
		MainSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		MainSlot->SetOffsets(FMargin(28.0f, 112.0f, 28.0f, 28.0f));
	}

	USizeBox* LeftSize = WidgetTree->ConstructWidget<USizeBox>();
	LeftSize->SetWidthOverride(410.0f);
	MainLayout->AddChild(LeftSize);
	UBorder* LeftPanel = WidgetTree->ConstructWidget<UBorder>();
	LeftPanel->SetBrushColor(PanelColor);
	LeftPanel->SetPadding(FMargin(22.0f));
	LeftSize->AddChild(LeftPanel);
	UVerticalBox* LeftColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	LeftPanel->AddChild(LeftColumn);

	AddSectionTitle(LeftColumn, TEXT("BODY"));
	UButton* PreviousBodyButton = nullptr;
	UButton* NextBodyButton = nullptr;
	UTextBlock* BodyValueWidget = nullptr;
	AddCycleRow(LeftColumn, TEXT("Body Type"), PreviousBodyButton, BodyValueWidget, NextBodyButton);
	BodyValue = BodyValueWidget;
	PreviousBodyButton->OnClicked.AddDynamic(this, &ThisClass::PreviousBody);
	NextBodyButton->OnClicked.AddDynamic(this, &ThisClass::NextBody);
	USlider* HeightSliderWidget = nullptr;
	USlider* HeadSizeSliderWidget = nullptr;
	USlider* ShoulderSliderWidget = nullptr;
	AddSliderRow(LeftColumn, TEXT("Height"), HeightSliderWidget);
	AddSliderRow(LeftColumn, TEXT("Head Size"), HeadSizeSliderWidget);
	AddSliderRow(LeftColumn, TEXT("Shoulders"), ShoulderSliderWidget);
	HeightSlider = HeightSliderWidget;
	HeadSizeSlider = HeadSizeSliderWidget;
	ShoulderSlider = ShoulderSliderWidget;
	HeightSlider->OnValueChanged.AddDynamic(this, &ThisClass::HeightChanged);
	HeadSizeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HeadSizeChanged);
	ShoulderSlider->OnValueChanged.AddDynamic(this, &ThisClass::ShoulderWidthChanged);

	AddSectionTitle(LeftColumn, TEXT("STYLE"));
	UButton* PreviousHairButton = nullptr;
	UButton* NextHairButton = nullptr;
	UTextBlock* HairValueWidget = nullptr;
	AddCycleRow(LeftColumn, TEXT("Hair"), PreviousHairButton, HairValueWidget, NextHairButton);
	HairValue = HairValueWidget;
	PreviousHairButton->OnClicked.AddDynamic(this, &ThisClass::PreviousHair);
	NextHairButton->OnClicked.AddDynamic(this, &ThisClass::NextHair);

	UButton* PreviousAccessoryButton = nullptr;
	UButton* NextAccessoryButton = nullptr;
	UTextBlock* AccessoryValueWidget = nullptr;
	AddCycleRow(LeftColumn, TEXT("Accessory"), PreviousAccessoryButton, AccessoryValueWidget, NextAccessoryButton);
	AccessoryValue = AccessoryValueWidget;
	PreviousAccessoryButton->OnClicked.AddDynamic(this, &ThisClass::PreviousAccessory);
	NextAccessoryButton->OnClicked.AddDynamic(this, &ThisClass::NextAccessory);

	USpacer* CenterSpace = WidgetTree->ConstructWidget<USpacer>();
	MainLayout->AddChild(CenterSpace);
	if (UHorizontalBoxSlot* CenterSlot = Cast<UHorizontalBoxSlot>(CenterSpace->Slot))
	{
		CenterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	USizeBox* RightSize = WidgetTree->ConstructWidget<USizeBox>();
	RightSize->SetWidthOverride(370.0f);
	MainLayout->AddChild(RightSize);
	UBorder* RightPanel = WidgetTree->ConstructWidget<UBorder>();
	RightPanel->SetBrushColor(PanelColor);
	RightPanel->SetPadding(FMargin(22.0f));
	RightSize->AddChild(RightPanel);
	UVerticalBox* RightColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	RightPanel->AddChild(RightColumn);

	AddSectionTitle(RightColumn, TEXT("COLORS"));
	TArray<UButton*> SkinButtons;
	AddColorRow(RightColumn, TEXT("Skin"), {
		FLinearColor(0.92f, 0.72f, 0.58f), FLinearColor(0.72f, 0.48f, 0.34f), FLinearColor(0.30f, 0.16f, 0.10f)}, SkinButtons);
	SkinButtons[0]->OnClicked.AddDynamic(this, &ThisClass::SkinLight);
	SkinButtons[1]->OnClicked.AddDynamic(this, &ThisClass::SkinMedium);
	SkinButtons[2]->OnClicked.AddDynamic(this, &ThisClass::SkinDeep);

	TArray<UButton*> HairButtons;
	AddColorRow(RightColumn, TEXT("Hair"), {
		FLinearColor(0.025f, 0.04f, 0.07f), FLinearColor(0.02f, 0.42f, 0.78f), FLinearColor(0.85f, 0.12f, 0.18f)}, HairButtons);
	HairButtons[0]->OnClicked.AddDynamic(this, &ThisClass::HairBlack);
	HairButtons[1]->OnClicked.AddDynamic(this, &ThisClass::HairBlue);
	HairButtons[2]->OnClicked.AddDynamic(this, &ThisClass::HairCoral);

	TArray<UButton*> OutfitButtons;
	AddColorRow(RightColumn, TEXT("Outfit"), {
		FLinearColor(0.02f, 0.55f, 0.95f), FLinearColor(0.95f, 0.72f, 0.04f), FLinearColor(0.88f, 0.08f, 0.12f)}, OutfitButtons);
	OutfitButtons[0]->OnClicked.AddDynamic(this, &ThisClass::OutfitCyan);
	OutfitButtons[1]->OnClicked.AddDynamic(this, &ThisClass::OutfitYellow);
	OutfitButtons[2]->OnClicked.AddDynamic(this, &ThisClass::OutfitRed);

	AddSectionTitle(RightColumn, TEXT("PREVIEW"));
	UHorizontalBox* RotateRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	RightColumn->AddChild(RotateRow);
	UButton* RotateLeftButton = CreateTextButton(TEXT("< ROTATE"));
	UButton* RotateRightButton = CreateTextButton(TEXT("ROTATE >"));
	RotateRow->AddChild(RotateLeftButton);
	RotateRow->AddChild(RotateRightButton);
	for (UWidget* Child : RotateRow->GetAllChildren())
	{
		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(Child->Slot))
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BoxSlot->SetPadding(FMargin(4.0f));
		}
	}
	RotateLeftButton->OnClicked.AddDynamic(this, &ThisClass::RotateLeft);
	RotateRightButton->OnClicked.AddDynamic(this, &ThisClass::RotateRight);

	UButton* RandomButton = CreateTextButton(TEXT("RANDOMIZE"));
	RightColumn->AddChild(RandomButton);
	RandomButton->OnClicked.AddDynamic(this, &ThisClass::Randomize);

	USpacer* FlexibleSpace = WidgetTree->ConstructWidget<USpacer>();
	RightColumn->AddChild(FlexibleSpace);
	if (UVerticalBoxSlot* FlexibleSlot = Cast<UVerticalBoxSlot>(FlexibleSpace->Slot))
	{
		FlexibleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	StatusText = CreateText(TEXT("Ready"), 14, MutedTextColor);
	RightColumn->AddChild(StatusText);

	UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	RightColumn->AddChild(ActionRow);
	UButton* ResetButton = CreateTextButton(TEXT("RESET"));
	UButton* SaveButton = CreateTextButton(TEXT("SAVE"));
	ActionRow->AddChild(ResetButton);
	ActionRow->AddChild(SaveButton);
	for (UWidget* Child : ActionRow->GetAllChildren())
	{
		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(Child->Slot))
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BoxSlot->SetPadding(FMargin(4.0f));
		}
	}
	ResetButton->OnClicked.AddDynamic(this, &ThisClass::Reset);
	SaveButton->OnClicked.AddDynamic(this, &ThisClass::Save);
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
	Button->SetBackgroundColor(FLinearColor(0.035f, 0.25f, 0.42f, 1.0f));
	Button->AddChild(CreateText(Text, 14, TextColor));
	return Button;
}

void UUECustomizationWidget::AddSectionTitle(UVerticalBox* Parent, const FString& Text)
{
	UTextBlock* Section = CreateText(Text, 19, CyanColor);
	Parent->AddChild(Section);
	if (UVerticalBoxSlot* SectionSlot = Cast<UVerticalBoxSlot>(Section->Slot))
	{
		SectionSlot->SetPadding(FMargin(0.0f, 15.0f, 0.0f, 8.0f));
	}
}

void UUECustomizationWidget::AddCycleRow(UVerticalBox* Parent, const FString& Label, UButton*& OutPrevious, UTextBlock*& OutValue, UButton*& OutNext)
{
	Parent->AddChild(CreateText(Label, 13, MutedTextColor));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChild(Row);
	OutPrevious = CreateTextButton(TEXT("<"));
	OutNext = CreateTextButton(TEXT(">"));
	OutValue = CreateText(TEXT("-"), 16, TextColor);
	OutValue->SetJustification(ETextJustify::Center);
	Row->AddChild(OutPrevious);
	Row->AddChild(OutValue);
	Row->AddChild(OutNext);
	if (UHorizontalBoxSlot* ValueSlot = Cast<UHorizontalBoxSlot>(OutValue->Slot))
	{
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UUECustomizationWidget::AddSliderRow(UVerticalBox* Parent, const FString& Label, USlider*& OutSlider)
{
	Parent->AddChild(CreateText(Label, 13, MutedTextColor));
	OutSlider = WidgetTree->ConstructWidget<USlider>();
	OutSlider->SetMinValue(0.0f);
	OutSlider->SetMaxValue(1.0f);
	OutSlider->SetStepSize(0.01f);
	OutSlider->SetSliderBarColor(FLinearColor(0.08f, 0.28f, 0.42f, 1.0f));
	OutSlider->SetSliderHandleColor(YellowColor);
	Parent->AddChild(OutSlider);
	if (UVerticalBoxSlot* SliderSlot = Cast<UVerticalBoxSlot>(OutSlider->Slot))
	{
		SliderSlot->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 11.0f));
	}
}

void UUECustomizationWidget::AddColorRow(UVerticalBox* Parent, const FString& Label, const TArray<FLinearColor>& Colors, TArray<UButton*>& OutButtons)
{
	Parent->AddChild(CreateText(Label, 13, MutedTextColor));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChild(Row);
	for (const FLinearColor& Color : Colors)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(64.0f);
		Size->SetHeightOverride(38.0f);
		Row->AddChild(Size);
		if (UHorizontalBoxSlot* SwatchSlot = Cast<UHorizontalBoxSlot>(Size->Slot))
		{
			SwatchSlot->SetPadding(FMargin(4.0f));
		}
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		Button->SetBackgroundColor(Color);
		Size->AddChild(Button);
		OutButtons.Add(Button);
	}
}

void UUECustomizationWidget::SynchronizeControls()
{
	if (!PreviewActor || !HeightSlider || !HeadSizeSlider || !ShoulderSlider)
	{
		return;
	}

	bSynchronizingControls = true;
	const FUECharacterCustomizationData& Appearance = PreviewActor->GetAppearance();
	HeightSlider->SetValue(Appearance.Height);
	HeadSizeSlider->SetValue(Appearance.HeadSize);
	ShoulderSlider->SetValue(Appearance.ShoulderWidth);
	RefreshValueLabels();
	bSynchronizingControls = false;
}

void UUECustomizationWidget::ApplyCurrentAppearance()
{
	if (PreviewActor)
	{
		PreviewActor->ApplyAppearance(PreviewActor->GetAppearance());
		RefreshValueLabels();
	}
}

void UUECustomizationWidget::RefreshValueLabels()
{
	if (!PreviewActor || !BodyValue || !HairValue || !AccessoryValue)
	{
		return;
	}

	const FUECharacterCustomizationData& Appearance = PreviewActor->GetAppearance();
	const TCHAR* BodyNames[] = {TEXT("Slim"), TEXT("Athletic"), TEXT("Heavy")};
	const TCHAR* HairNames[] = {TEXT("Round"), TEXT("Block"), TEXT("Spike"), TEXT("Short")};
	const TCHAR* AccessoryNames[] = {TEXT("None"), TEXT("Collar"), TEXT("Core")};
	BodyValue->SetText(FText::FromString(BodyNames[FMath::Clamp(static_cast<int32>(Appearance.BodyPreset), 0, 2)]));
	HairValue->SetText(FText::FromString(HairNames[FMath::Clamp(Appearance.HairStyle, 0, 3)]));
	AccessoryValue->SetText(FText::FromString(AccessoryNames[FMath::Clamp(Appearance.AccessoryStyle, 0, 2)]));
}

void UUECustomizationWidget::PreviousBody()
{
	if (!PreviewActor) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	Data.BodyPreset = static_cast<EUEBodyPreset>((static_cast<int32>(Data.BodyPreset) + 2) % 3);
	PreviewActor->ApplyAppearance(Data);
	RefreshValueLabels();
}

void UUECustomizationWidget::NextBody()
{
	if (!PreviewActor) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	Data.BodyPreset = static_cast<EUEBodyPreset>((static_cast<int32>(Data.BodyPreset) + 1) % 3);
	PreviewActor->ApplyAppearance(Data);
	RefreshValueLabels();
}

void UUECustomizationWidget::PreviousHair()
{
	if (!PreviewActor) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	Data.HairStyle = (Data.HairStyle + 3) % 4;
	PreviewActor->ApplyAppearance(Data);
	RefreshValueLabels();
}

void UUECustomizationWidget::NextHair()
{
	if (!PreviewActor) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	Data.HairStyle = (Data.HairStyle + 1) % 4;
	PreviewActor->ApplyAppearance(Data);
	RefreshValueLabels();
}

void UUECustomizationWidget::PreviousAccessory()
{
	if (!PreviewActor) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	Data.AccessoryStyle = (Data.AccessoryStyle + 2) % 3;
	PreviewActor->ApplyAppearance(Data);
	RefreshValueLabels();
}

void UUECustomizationWidget::NextAccessory()
{
	if (!PreviewActor) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance();
	Data.AccessoryStyle = (Data.AccessoryStyle + 1) % 3;
	PreviewActor->ApplyAppearance(Data);
	RefreshValueLabels();
}

void UUECustomizationWidget::HeightChanged(float Value)
{
	if (!PreviewActor || bSynchronizingControls) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance(); Data.Height = Value; PreviewActor->ApplyAppearance(Data);
}

void UUECustomizationWidget::HeadSizeChanged(float Value)
{
	if (!PreviewActor || bSynchronizingControls) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance(); Data.HeadSize = Value; PreviewActor->ApplyAppearance(Data);
}

void UUECustomizationWidget::ShoulderWidthChanged(float Value)
{
	if (!PreviewActor || bSynchronizingControls) return;
	FUECharacterCustomizationData Data = PreviewActor->GetAppearance(); Data.ShoulderWidth = Value; PreviewActor->ApplyAppearance(Data);
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
	PreviewActor->ResetAppearance(); SynchronizeControls();
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("Reset to defaults")));
}

void UUECustomizationWidget::Randomize()
{
	if (!PreviewActor) return;
	PreviewActor->RandomizeAppearance(); SynchronizeControls();
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("Randomized")));
}

#define UE_SET_CUSTOMIZATION_COLOR(FunctionName, FieldName, R, G, B) \
	void UUECustomizationWidget::FunctionName() \
	{ \
		if (!PreviewActor) return; \
		FUECharacterCustomizationData Data = PreviewActor->GetAppearance(); \
		Data.FieldName = FLinearColor(R, G, B, 1.0f); \
		PreviewActor->ApplyAppearance(Data); \
	}

UE_SET_CUSTOMIZATION_COLOR(SkinLight, SkinColor, 0.92f, 0.72f, 0.58f)
UE_SET_CUSTOMIZATION_COLOR(SkinMedium, SkinColor, 0.72f, 0.48f, 0.34f)
UE_SET_CUSTOMIZATION_COLOR(SkinDeep, SkinColor, 0.30f, 0.16f, 0.10f)
UE_SET_CUSTOMIZATION_COLOR(HairBlack, HairColor, 0.025f, 0.04f, 0.07f)
UE_SET_CUSTOMIZATION_COLOR(HairBlue, HairColor, 0.02f, 0.42f, 0.78f)
UE_SET_CUSTOMIZATION_COLOR(HairCoral, HairColor, 0.85f, 0.12f, 0.18f)
UE_SET_CUSTOMIZATION_COLOR(OutfitCyan, OutfitColor, 0.02f, 0.55f, 0.95f)
UE_SET_CUSTOMIZATION_COLOR(OutfitYellow, OutfitColor, 0.95f, 0.72f, 0.04f)
UE_SET_CUSTOMIZATION_COLOR(OutfitRed, OutfitColor, 0.88f, 0.08f, 0.12f)

#undef UE_SET_CUSTOMIZATION_COLOR
