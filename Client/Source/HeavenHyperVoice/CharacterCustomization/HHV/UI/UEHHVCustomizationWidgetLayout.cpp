#include "UEHHVCustomizationWidget.h"
#include "UEHHVCustomizationWidgetPrivate.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

using namespace UEHHVCustomizationWidgetPrivate;
bool UUEHHVCustomizationWidget::BindDesignerInterface()
{
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		return false;
	}

	UWidget* DesignerCanvas = WidgetTree->RootWidget;
	CategoryList = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Designer_CategoryList")));
	OptionScroll = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("Designer_OptionScroll")));
	ColorControls = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Designer_ColorControls")));
	OptionTitle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Designer_OptionTitle")));
	OptionCount = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Designer_OptionCount")));
	StatusText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Designer_StatusText")));
	StartButton = FindDesignerStartButton();
	if (UWidget* LegacyScaleWidget = WidgetTree->FindWidget(TEXT("Designer_ScaleControls")))
	{
		// 예전 WBP에 남아 있는 체격 슬라이더 영역은 더 이상 쓰지 않는다.
		LegacyScaleWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UTextBlock* DesignerTitle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Designer_Title"))))
	{
		DesignerTitle->SetText(FText::FromString(TEXT("HeavenHyperVoice CHARACTER CREATION")));
		DesignerTitle->SetColorAndOpacity(FSlateColor(TextColor));
	}
	if (UBorder* LeftPanel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Designer_LeftOptionsPanel"))))
	{
		LeftPanel->SetBrushColor(PanelColor);
		LeftPanel->SetPadding(FMargin(16.0f));
	}
	if (UBorder* RightPanel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Designer_RightParametersPanel"))))
	{
		RightPanel->SetBrushColor(PanelColor);
		RightPanel->SetPadding(FMargin(16.0f));
	}

	if (!DesignerCanvas || !CategoryList || !OptionScroll || !ColorControls)
	{
		return false;
	}

	ColorButtons.Empty();

	if (!OptionGrid)
	{
		OptionGrid = Cast<UUniformGridPanel>(WidgetTree->FindWidget(TEXT("Designer_OptionGrid")));
	}

	if (!OptionGrid)
	{
		OptionGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(
			UUniformGridPanel::StaticClass(),
			TEXT("Designer_OptionGrid"));
	}

	// 화면 배치는 WBP 디자이너 캔버스가 맡고, C++는 데이터 목록만 채운다.
	OptionGrid->RemoveFromParent();
	OptionScroll->ClearChildren();
	OptionScroll->AddChild(OptionGrid);

	RebuildParameterControls();
	if (!StartButton)
	{
		UE_LOG(LogTemp, Warning, TEXT("Character customization WBP is missing Designer_StartButton."));
	}
	if (UTextBlock* StartText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Designer_StartButtonText"))))
	{
		StartText->SetText(FText::FromString(TEXT("게임 시작")));
	}
	BindStartButton();

	return true;
}

void UUEHHVCustomizationWidget::RebuildCategories()
{
	if (!CategoryList)
	{
		return;
	}
	CategoryList->ClearChildren();

	for (const FCategoryEntry& Entry : GetCategoryEntries())
	{
		const int32 Count = GetVisibleOptionCount(Entry.Category);
		UUEHHVOptionButton* Button = WidgetTree->ConstructWidget<UUEHHVOptionButton>();
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

void UUEHHVCustomizationWidget::RebuildOptions()
{
	if (!OptionGrid)
	{
		return;
	}

	OptionGrid->ClearChildren();
	const int32 Count = GetVisibleOptionCount(CurrentCategory);
	if (OptionTitle)
	{
		OptionTitle->SetText(FText::FromString(GetCategoryLabel(CurrentCategory)));
		OptionTitle->SetColorAndOpacity(FSlateColor(TextColor));
	}
	if (OptionCount)
	{
		OptionCount->SetText(FText::FromString(FString::Printf(TEXT("%d개 항목"), Count)));
		OptionCount->SetColorAndOpacity(FSlateColor(MutedTextColor));
	}

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

		UUEHHVOptionButton* Button = WidgetTree->ConstructWidget<UUEHHVOptionButton>();
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
		UTextBlock* Empty = CreateText(TEXT("이 카테고리에 맞는 항목이 없어."), 13, MutedTextColor);
		Empty->SetAutoWrapText(true);
		OptionGrid->AddChildToUniformGrid(Empty, 0, 0);
	}
}

void UUEHHVCustomizationWidget::RebuildParameterControls()
{
	if (!ColorControls)
	{
		return;
	}

	ColorButtons.Empty();
	ColorControls->ClearChildren();

	AddSectionTitle(ColorControls, TEXT("색상"));
	switch (CurrentCategory)
	{
	case EUEHHVCustomizationCategory::Body:
	case EUEHHVCustomizationCategory::Head:
		AddPaletteRow(ColorControls, TEXT("피부색"), EUEHHVColorChannel::Skin);
		break;
	case EUEHHVCustomizationCategory::Hair:
		AddPaletteRow(ColorControls, TEXT("머리 색상"), EUEHHVColorChannel::Hair);
		break;
	case EUEHHVCustomizationCategory::Eyes:
		AddPaletteRow(ColorControls, TEXT("눈 색상"), EUEHHVColorChannel::Eye);
		break;
	default:
	{
		UTextBlock* OriginalMaterialText = CreateText(
			TEXT("의상은 원본 머티리얼을 그대로 사용해."),
			11,
			MutedTextColor);
		OriginalMaterialText->SetAutoWrapText(true);
		ColorControls->AddChild(OriginalMaterialText);
		break;
	}
	}

}

void UUEHHVCustomizationWidget::SynchronizeControls()
{
	const TGuardValue<bool> SynchronizeGuard(bSynchronizingControls, true);
	RefreshFromPreview();

	if (StatusText)
	{
		int32 SelectedVisibleIndex = 0;
		const int32 SelectedActualIndex = GetSelectedIndex(CurrentCategory);
	if (CurrentCategory == EUEHHVCustomizationCategory::Body && GetOptionCount(CurrentCategory) > 2)
	{
		SelectedVisibleIndex = FMath::Max(0, SelectedActualIndex - 1);
	}
	else if (CurrentCategory == EUEHHVCustomizationCategory::BodyEquipment && GetOptionCount(CurrentCategory) > 1)
	{
		SelectedVisibleIndex = FMath::Clamp(
			SelectedActualIndex - FirstVisibleBodyEquipmentIndex,
			0,
			FMath::Max(0, GetVisibleOptionCount(CurrentCategory) - 1));
	}
	else
	{
		SelectedVisibleIndex = FMath::Clamp(SelectedActualIndex, 0, FMath::Max(0, GetVisibleOptionCount(CurrentCategory) - 1));
		}
		StatusText->SetText(FText::FromString(FString::Printf(
			TEXT("%s #%02d"),
			*GetCategoryLabel(CurrentCategory),
			SelectedVisibleIndex + 1)));
		StatusText->SetColorAndOpacity(FSlateColor(MutedTextColor));
	}
}

UTextBlock* UUEHHVCustomizationWidget::CreateText(
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

void UUEHHVCustomizationWidget::AddSectionTitle(UVerticalBox* Parent, const FString& Text)
{
	UTextBlock* Title = CreateText(Text, 15, AccentColor);
	Parent->AddChild(Title);
	CastChecked<UVerticalBoxSlot>(Title->Slot)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 4.0f));
}

void UUEHHVCustomizationWidget::AddPaletteRow(
	UVerticalBox* Parent,
	const FString& Label,
	EUEHHVColorChannel Channel)
{
	UTextBlock* RowLabel = CreateText(Label, 12, TextColor);
	Parent->AddChild(RowLabel);
	CastChecked<UVerticalBoxSlot>(RowLabel->Slot)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 2.0f));

	USizeBox* CurrentSize = WidgetTree->ConstructWidget<USizeBox>();
	CurrentSize->SetWidthOverride(216.0f);
	CurrentSize->SetHeightOverride(22.0f);
	UBorder* CurrentColor = WidgetTree->ConstructWidget<UBorder>();
	CurrentColor->SetBrushColor(GetChannelColor(Channel).GetClamped());
	CurrentSize->AddChild(CurrentColor);
	Parent->AddChild(CurrentSize);
	CastChecked<UVerticalBoxSlot>(CurrentSize->Slot)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	const TArray<FLinearColor> Colors = GetPaletteColors(Channel);

	// 숫자 RGB 입력 대신, 눈으로 고르는 큰 색상 매트릭스와 아래 색상 줄을 쓴다.
	USizeBox* PickerSize = WidgetTree->ConstructWidget<USizeBox>();
	PickerSize->SetWidthOverride(216.0f);
	PickerSize->SetHeightOverride(148.0f);
	UBorder* PickerFrame = WidgetTree->ConstructWidget<UBorder>();
	PickerFrame->SetBrushColor(PanelSecondary);
	PickerFrame->SetPadding(FMargin(4.0f));
	PickerSize->AddChild(PickerFrame);
	Parent->AddChild(PickerSize);
	CastChecked<UVerticalBoxSlot>(PickerSize->Slot)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));

	UUniformGridPanel* MatrixGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	PickerFrame->AddChild(MatrixGrid);

	const int32 MatrixColumns = 12;
	const int32 MaxMatrixColors = FMath::Min(Colors.Num(), 96);
	for (int32 Index = 0; Index < MaxMatrixColors; ++Index)
	{
		USizeBox* SwatchSize = WidgetTree->ConstructWidget<USizeBox>();
		SwatchSize->SetWidthOverride(17.0f);
		SwatchSize->SetHeightOverride(17.0f);
		MatrixGrid->AddChildToUniformGrid(SwatchSize, Index / MatrixColumns, Index % MatrixColumns);

		UUEHHVColorButton* Swatch = WidgetTree->ConstructWidget<UUEHHVColorButton>();
		Swatch->Configure(this, Channel, Colors[Index]);
		SwatchSize->AddChild(Swatch);
		ColorButtons.Add(Swatch);
	}

	UUniformGridPanel* HueStrip = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Parent->AddChild(HueStrip);
	CastChecked<UVerticalBoxSlot>(HueStrip->Slot)->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 6.0f));

	TArray<FLinearColor> StripColors;
	if (Channel == EUEHHVColorChannel::Skin)
	{
		for (uint8 Hue : {12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78})
		{
			AddUniqueColor(StripColors, FLinearColor::MakeFromHSV8(Hue, 130, 230));
		}
	}
	else
	{
		for (uint8 Hue : {0, 18, 36, 54, 72, 96, 120, 144, 168, 192, 216, 238})
		{
			AddUniqueColor(StripColors, FLinearColor::MakeFromHSV8(Hue, 210, 235));
		}
	}

	for (int32 Index = 0; Index < StripColors.Num(); ++Index)
	{
		USizeBox* StripSwatchSize = WidgetTree->ConstructWidget<USizeBox>();
		StripSwatchSize->SetWidthOverride(18.0f);
		StripSwatchSize->SetHeightOverride(18.0f);
		HueStrip->AddChildToUniformGrid(StripSwatchSize, 0, Index);

		UUEHHVColorButton* StripSwatch = WidgetTree->ConstructWidget<UUEHHVColorButton>();
		StripSwatch->Configure(this, Channel, StripColors[Index]);
		StripSwatchSize->AddChild(StripSwatch);
		ColorButtons.Add(StripSwatch);
	}
}

UButton* UUEHHVCustomizationWidget::FindDesignerStartButton() const
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	for (const TCHAR* Name : {TEXT("Designer_StartButton"), TEXT("Button_Start"), TEXT("StartButton"), TEXT("Designer_GameStartButton")})
	{
		if (UButton* Button = Cast<UButton>(WidgetTree->FindWidget(Name)))
		{
			return Button;
		}
	}
	return nullptr;
}

void UUEHHVCustomizationWidget::BindStartButton()
{
	if (!StartButton)
	{
		return;
	}

	StartButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleStartClicked);
	StartButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartClicked);
}


