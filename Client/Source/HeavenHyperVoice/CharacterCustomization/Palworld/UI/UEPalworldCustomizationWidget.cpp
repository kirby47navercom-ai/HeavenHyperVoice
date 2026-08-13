#include "UEPalworldCustomizationWidget.h"

#include "../Preview/UEPalworldCustomizationPreviewActor.h"
#include "../../../System/UEGameInstance.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	const FLinearColor PanelColor(0.025f, 0.035f, 0.045f, 0.96f);
	const FLinearColor PanelSecondary(0.055f, 0.065f, 0.075f, 0.98f);
	const FLinearColor TileColor(0.095f, 0.11f, 0.125f, 1.0f);
	const FLinearColor TextColor(0.94f, 0.95f, 0.96f, 1.0f);
	const FLinearColor MutedTextColor(0.62f, 0.66f, 0.70f, 1.0f);
	const FLinearColor AccentColor(0.13f, 0.72f, 0.66f, 1.0f);
	const FLinearColor SelectedColor(0.92f, 0.55f, 0.16f, 1.0f);

	constexpr int32 WidgetMaxVisibleOutfits = 14;

	void AddUniqueColor(TArray<FLinearColor>& Colors, const FLinearColor& Color)
	{
		const FLinearColor Clamped = Color.GetClamped();
		for (const FLinearColor& Existing : Colors)
		{
			if (Existing.Equals(Clamped, 0.015f))
			{
				return;
			}
		}
		Colors.Add(Clamped);
	}

	void AppendVisualPickerColors(TArray<FLinearColor>& Colors, EUEPalworldColorChannel Channel)
	{
		if (Channel == EUEPalworldColorChannel::Skin)
		{
			for (float Value : {1.0f, 0.92f, 0.84f, 0.76f, 0.66f, 0.56f})
			{
				for (float Saturation : {0.12f, 0.22f, 0.34f, 0.46f, 0.58f, 0.70f})
				{
					AddUniqueColor(Colors, FLinearColor::MakeFromHSV8(22, uint8(Saturation * 255.0f), uint8(Value * 255.0f)));
				}
			}
			return;
		}

		if (Channel == EUEPalworldColorChannel::Hair)
		{
			for (float Value : {0.18f, 0.32f, 0.48f, 0.68f, 0.86f})
			{
				for (uint8 Hue : {0, 14, 24, 34, 48, 64, 96, 128, 160, 190, 216, 238})
				{
					AddUniqueColor(Colors, FLinearColor::MakeFromHSV8(Hue, 185, uint8(Value * 255.0f)));
				}
			}
			return;
		}

		for (float Value : {0.30f, 0.46f, 0.62f, 0.78f})
		{
			for (uint8 Hue : {0, 24, 42, 74, 105, 135, 160, 186, 210, 232})
			{
				AddUniqueColor(Colors, FLinearColor::MakeFromHSV8(Hue, 180, uint8(Value * 255.0f)));
			}
		}
		AddUniqueColor(Colors, FLinearColor(0.08f, 0.08f, 0.08f));
		AddUniqueColor(Colors, FLinearColor(0.72f, 0.72f, 0.72f));
	}

	struct FCategoryEntry
	{
		EUEPalworldCustomizationCategory Category;
		const TCHAR* Label;
	};

	TConstArrayView<FCategoryEntry> GetCategoryEntries()
	{
		static const TArray<FCategoryEntry> Entries = {
			{EUEPalworldCustomizationCategory::Body, TEXT("몸")},
			{EUEPalworldCustomizationCategory::Head, TEXT("얼굴")},
			{EUEPalworldCustomizationCategory::Hair, TEXT("머리카락")},
			{EUEPalworldCustomizationCategory::Eyes, TEXT("눈")},
			{EUEPalworldCustomizationCategory::BodyEquipment, TEXT("의상")}
		};
		return Entries;
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

void UUEPalworldColorButton::Configure(
	UUEPalworldCustomizationWidget* InOwner,
	EUEPalworldColorChannel InChannel,
	FLinearColor InColor)
{
	OwnerWidget = InOwner;
	Channel = InChannel;
	Color = InColor.GetClamped();
	SetBackgroundColor(Color);
	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UUEPalworldColorButton::HandleClicked()
{
	if (OwnerWidget)
	{
		OwnerWidget->SelectColor(Channel, Color);
	}
}

void UUEPalworldScaleSlider::Configure(UUEPalworldCustomizationWidget* InOwner, EUEPalworldScaleChannel InChannel)
{
	OwnerWidget = InOwner;
	Channel = InChannel;
	SetMinValue(0.0f);
	SetMaxValue(100.0f);
	SetStepSize(1.0f);
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
	RebuildParameterControls();
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

void UUEPalworldCustomizationWidget::StartWithCurrentAppearance()
{
	RefreshFromPreview();
	if (UUEGameInstance* UEGameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		UEGameInstance->SetPendingPalworldAppearance(CachedAppearance);
	}

	if (!StartLevelName.IsNone())
	{
		FString LevelNameString = StartLevelName.ToString();
		if (LevelNameString == TEXT("PlayerTestLevel"))
		{
			LevelNameString = TEXT("/Game/Level/PlayerTestLevel");
		}

		// 커마 레벨에서 넘어갈 때는 맵의 기존 월드 세팅보다 Palworld 캐릭터용 게임모드를 우선 사용한다.
		UGameplayStatics::OpenLevel(
			this,
			FName(*LevelNameString),
			true,
			TEXT("game=/Script/HeavenHyperVoice.UEGameModeBase"));
	}
}

void UUEPalworldCustomizationWidget::OpenCategory(EUEPalworldCustomizationCategory Category)
{
	CurrentCategory = Category;
	RebuildCategories();
	RebuildOptions();
	RebuildParameterControls();
	SynchronizeControls();
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
	if (!BindDesignerInterface())
	{
		UE_LOG(LogTemp, Warning, TEXT("Palworld customization WBP designer canvas is missing required widgets."));
	}
	return Super::RebuildWidget();
}

void UUEPalworldCustomizationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("DisableAllScreenMessages"));
	RefreshFromPreview();
	BindDesignerInterface();
	RebuildCategories();
	RebuildOptions();
	SynchronizeControls();
}

FReply UUEPalworldCustomizationWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!PreviewActor || !IsPointerOverPreviewArea(InGeometry, InMouseEvent))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bRotatingPreview = true;
		bPanningPreview = false;
		LastPointerScreenPosition = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton ||
		InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bRotatingPreview = false;
		bPanningPreview = true;
		LastPointerScreenPosition = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UUEPalworldCustomizationWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bRotatingPreview || bPanningPreview)
	{
		bRotatingPreview = false;
		bPanningPreview = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UUEPalworldCustomizationWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!PreviewActor || (!bRotatingPreview && !bPanningPreview))
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const FVector2D CurrentPosition = InMouseEvent.GetScreenSpacePosition();
	const FVector2D Delta = CurrentPosition - LastPointerScreenPosition;
	LastPointerScreenPosition = CurrentPosition;

	if (bRotatingPreview)
	{
		PreviewActor->AddPreviewYaw(Delta.X * 0.28f);
	}
	else if (bPanningPreview)
	{
		PreviewActor->AddPreviewPan(Delta);
	}

	return FReply::Handled();
}

FReply UUEPalworldCustomizationWidget::NativeOnMouseWheel(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!PreviewActor || !IsPointerOverPreviewArea(InGeometry, InMouseEvent))
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}

	PreviewActor->AddPreviewZoom(-InMouseEvent.GetWheelDelta() * 0.12f);
	return FReply::Handled();
}

bool UUEPalworldCustomizationWidget::BindDesignerInterface()
{
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		return false;
	}

	UWidget* DesignerCanvas = WidgetTree->FindWidget(TEXT("PalworldDesignerCanvas"));
	CategoryList = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Designer_CategoryList")));
	OptionScroll = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("Designer_OptionScroll")));
	ColorControls = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Designer_ColorControls")));
	ScaleControls = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Designer_ScaleControls")));
	OptionTitle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Designer_OptionTitle")));
	OptionCount = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Designer_OptionCount")));
	StatusText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Designer_StatusText")));
	StartButton = FindDesignerStartButton();
	if (UTextBlock* DesignerTitle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Designer_Title"))))
	{
		DesignerTitle->SetText(FText::FromString(TEXT("Palworld CHARACTER CREATION")));
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

	if (!DesignerCanvas || !CategoryList || !OptionScroll || !ColorControls || !ScaleControls)
	{
		return false;
	}

	ColorButtons.Empty();
	ScaleSliders.Empty();

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
		UE_LOG(LogTemp, Warning, TEXT("Palworld customization WBP is missing Designer_StartButton."));
	}
	if (UTextBlock* StartText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Designer_StartButtonText"))))
	{
		StartText->SetText(FText::FromString(TEXT("게임 시작")));
	}
	BindStartButton();

	return true;
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
		UTextBlock* Empty = CreateText(TEXT("이 카테고리에 맞는 항목이 없어."), 13, MutedTextColor);
		Empty->SetAutoWrapText(true);
		OptionGrid->AddChildToUniformGrid(Empty, 0, 0);
	}
}

void UUEPalworldCustomizationWidget::RebuildParameterControls()
{
	if (!ColorControls || !ScaleControls)
	{
		return;
	}

	ColorButtons.Empty();
	ScaleSliders.Empty();
	ColorControls->ClearChildren();
	ScaleControls->ClearChildren();

	AddSectionTitle(ColorControls, TEXT("색상"));
	switch (CurrentCategory)
	{
	case EUEPalworldCustomizationCategory::Body:
	case EUEPalworldCustomizationCategory::Head:
		AddPaletteRow(ColorControls, TEXT("피부색"), EUEPalworldColorChannel::Skin);
		break;
	case EUEPalworldCustomizationCategory::Hair:
		AddPaletteRow(ColorControls, TEXT("머리 색상"), EUEPalworldColorChannel::Hair);
		break;
	case EUEPalworldCustomizationCategory::Eyes:
		AddPaletteRow(ColorControls, TEXT("눈 색상"), EUEPalworldColorChannel::Eye);
		break;
	default:
	{
		UTextBlock* OriginalMaterialText = CreateText(
			TEXT("의상은 원본 Palworld 머티리얼을 그대로 사용해."),
			11,
			MutedTextColor);
		OriginalMaterialText->SetAutoWrapText(true);
		ColorControls->AddChild(OriginalMaterialText);
		break;
	}
	}

	if (CurrentCategory == EUEPalworldCustomizationCategory::Body)
	{
		AddSectionTitle(ScaleControls, TEXT("체격 설정"));
		AddScaleRow(ScaleControls, TEXT("몸통 크기"), EUEPalworldScaleChannel::TorsoSize);
		AddScaleRow(ScaleControls, TEXT("팔 크기"), EUEPalworldScaleChannel::ArmSize);
		AddScaleRow(ScaleControls, TEXT("다리 크기"), EUEPalworldScaleChannel::LegSize);
	}
}

void UUEPalworldCustomizationWidget::SynchronizeControls()
{
	const TGuardValue<bool> SynchronizeGuard(bSynchronizingControls, true);
	RefreshFromPreview();

	for (const TObjectPtr<UUEPalworldScaleSlider>& Slider : ScaleSliders)
	{
		if (Slider)
		{
			Slider->SetValue(GetScaleValue(Slider->GetChannel()));
		}
	}

	if (StatusText)
	{
		int32 SelectedVisibleIndex = 0;
		const int32 SelectedActualIndex = GetSelectedIndex(CurrentCategory);
	if (CurrentCategory == EUEPalworldCustomizationCategory::Body && GetOptionCount(CurrentCategory) > 2)
	{
		SelectedVisibleIndex = FMath::Max(0, SelectedActualIndex - 1);
	}
	else if (CurrentCategory == EUEPalworldCustomizationCategory::BodyEquipment && GetOptionCount(CurrentCategory) > 1)
	{
		SelectedVisibleIndex = FMath::Max(0, SelectedActualIndex - 1);
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
	default:
		return 0;
	}
}

int32 UUEPalworldCustomizationWidget::GetVisibleOptionCount(EUEPalworldCustomizationCategory Category) const
{
	const int32 RawCount = GetOptionCount(Category);
	if (Category == EUEPalworldCustomizationCategory::Body && RawCount > 2)
	{
		return 2;
	}
	if (Category == EUEPalworldCustomizationCategory::BodyEquipment && RawCount > 1)
	{
		return FMath::Min(WidgetMaxVisibleOutfits, RawCount - 1);
	}
	return RawCount;
}

int32 UUEPalworldCustomizationWidget::GetActualOptionIndex(
	EUEPalworldCustomizationCategory Category,
	int32 VisibleIndex) const
{
	if (Category == EUEPalworldCustomizationCategory::Body && GetOptionCount(Category) > 2)
	{
		return FMath::Clamp(VisibleIndex + 1, 1, GetOptionCount(Category) - 1);
	}
	if (Category == EUEPalworldCustomizationCategory::BodyEquipment && GetOptionCount(Category) > 1)
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
	default:
		return FLinearColor::White;
	}
}

TArray<FLinearColor> UUEPalworldCustomizationWidget::GetPaletteColors(EUEPalworldColorChannel Channel) const
{
	TArray<FLinearColor> Colors;
	auto AppendCatalogColors = [&Colors](const TArray<FLinearColor>& SourceColors)
	{
		for (const FLinearColor& Color : SourceColors)
		{
			AddUniqueColor(Colors, Color);
		}
	};

	if (Catalog)
	{
		switch (Channel)
		{
		case EUEPalworldColorChannel::Skin:
			if (!Catalog->SkinColors.IsEmpty())
			{
				AppendCatalogColors(Catalog->SkinColors);
			}
			break;
		case EUEPalworldColorChannel::Hair:
			if (!Catalog->HairColors.IsEmpty())
			{
				AppendCatalogColors(Catalog->HairColors);
			}
			break;
		case EUEPalworldColorChannel::Eye:
			if (!Catalog->EyeColors.IsEmpty())
			{
				AppendCatalogColors(Catalog->EyeColors);
			}
			break;
		default:
			break;
		}
	}

	AppendVisualPickerColors(Colors, Channel);
	if (!Colors.IsEmpty())
	{
		return Colors;
	}

	if (Channel == EUEPalworldColorChannel::Skin)
	{
		return {
			FLinearColor(1.0f, 0.74f, 0.62f),
			FLinearColor(0.90f, 0.55f, 0.40f),
			FLinearColor(0.62f, 0.36f, 0.24f),
			FLinearColor(0.36f, 0.22f, 0.16f)
		};
	}
	if (Channel == EUEPalworldColorChannel::Hair)
	{
		return {
			FLinearColor(0.05f, 0.035f, 0.025f),
			FLinearColor(0.22f, 0.10f, 0.045f),
			FLinearColor(0.65f, 0.32f, 0.12f),
			FLinearColor(0.84f, 0.62f, 0.38f),
			FLinearColor(0.58f, 0.08f, 0.08f),
			FLinearColor(0.28f, 0.42f, 0.72f)
		};
	}
	return {
		FLinearColor(0.18f, 0.40f, 0.22f),
		FLinearColor(0.18f, 0.36f, 0.62f),
		FLinearColor(0.48f, 0.32f, 0.18f),
		FLinearColor(0.70f, 0.72f, 0.72f),
		FLinearColor(0.08f, 0.08f, 0.08f)
	};
}

float UUEPalworldCustomizationWidget::GetScaleValue(EUEPalworldScaleChannel Channel) const
{
	const FUEPalworldAppearance& Appearance = GetAppearance();
	auto ToPercent = [](float Volume)
	{
		return FMath::Clamp((Volume + 1.0f) * 50.0f, 0.0f, 100.0f);
	};
	switch (Channel)
	{
	case EUEPalworldScaleChannel::TorsoSize:
		return ToPercent(Appearance.TorsoVolume);
	case EUEPalworldScaleChannel::ArmSize:
		return ToPercent(Appearance.ArmVolume);
	case EUEPalworldScaleChannel::LegSize:
		return ToPercent(Appearance.LegVolume);
	default:
		return 50.0f;
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

void UUEPalworldCustomizationWidget::AddSectionTitle(UVerticalBox* Parent, const FString& Text)
{
	UTextBlock* Title = CreateText(Text, 15, AccentColor);
	Parent->AddChild(Title);
	CastChecked<UVerticalBoxSlot>(Title->Slot)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 4.0f));
}

void UUEPalworldCustomizationWidget::AddPaletteRow(
	UVerticalBox* Parent,
	const FString& Label,
	EUEPalworldColorChannel Channel)
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

		UUEPalworldColorButton* Swatch = WidgetTree->ConstructWidget<UUEPalworldColorButton>();
		Swatch->Configure(this, Channel, Colors[Index]);
		SwatchSize->AddChild(Swatch);
		ColorButtons.Add(Swatch);
	}

	UUniformGridPanel* HueStrip = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Parent->AddChild(HueStrip);
	CastChecked<UVerticalBoxSlot>(HueStrip->Slot)->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 6.0f));

	TArray<FLinearColor> StripColors;
	if (Channel == EUEPalworldColorChannel::Skin)
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

		UUEPalworldColorButton* StripSwatch = WidgetTree->ConstructWidget<UUEPalworldColorButton>();
		StripSwatch->Configure(this, Channel, StripColors[Index]);
		StripSwatchSize->AddChild(StripSwatch);
		ColorButtons.Add(StripSwatch);
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

UButton* UUEPalworldCustomizationWidget::FindDesignerStartButton() const
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

void UUEPalworldCustomizationWidget::BindStartButton()
{
	if (!StartButton)
	{
		return;
	}

	StartButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleStartClicked);
	StartButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartClicked);
}

bool UUEPalworldCustomizationWidget::IsPointerOverPreviewArea(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent) const
{
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D LocalSize = InGeometry.GetLocalSize();
	if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
	{
		return false;
	}

	// 좌우 UI 패널을 제외한 중앙 프리뷰 영역에서 회전/이동/확대 입력을 받는다.
	return LocalPosition.X > LocalSize.X * 0.16f &&
		LocalPosition.X < LocalSize.X * 0.86f &&
		LocalPosition.Y > LocalSize.Y * 0.02f &&
		LocalPosition.Y < LocalSize.Y * 0.98f;
}

void UUEPalworldCustomizationWidget::HandleStartClicked()
{
	StartWithCurrentAppearance();
}

FString UUEPalworldCustomizationWidget::GetCategoryLabel(EUEPalworldCustomizationCategory Category)
{
	switch (Category)
	{
	case EUEPalworldCustomizationCategory::Body:
		return TEXT("몸");
	case EUEPalworldCustomizationCategory::Head:
		return TEXT("얼굴");
	case EUEPalworldCustomizationCategory::Hair:
		return TEXT("머리카락");
	case EUEPalworldCustomizationCategory::Eyes:
		return TEXT("눈");
	case EUEPalworldCustomizationCategory::BodyEquipment:
		return TEXT("의상");
	default:
		return TEXT("항목");
	}
}
