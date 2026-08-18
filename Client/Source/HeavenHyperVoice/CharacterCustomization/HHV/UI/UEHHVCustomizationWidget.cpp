#include "UEHHVCustomizationWidget.h"

#include "../Preview/UEHHVCustomizationPreviewActor.h"
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

#include "UEHHVCustomizationWidgetPrivate.h"

using namespace UEHHVCustomizationWidgetPrivate;

void UUEHHVOptionButton::Configure(
	UUEHHVCustomizationWidget* InOwner,
	EUEHHVCustomizationCategory InCategory,
	int32 InIndex)
{
	OwnerWidget = InOwner;
	Category = InCategory;
	Index = InIndex;
	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UUEHHVOptionButton::HandleClicked()
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

void UUEHHVColorButton::Configure(
	UUEHHVCustomizationWidget* InOwner,
	EUEHHVColorChannel InChannel,
	FLinearColor InColor)
{
	OwnerWidget = InOwner;
	Channel = InChannel;
	Color = InColor.GetClamped();
	SetBackgroundColor(Color);
	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UUEHHVColorButton::HandleClicked()
{
	if (OwnerWidget)
	{
		OwnerWidget->SelectColor(Channel, Color);
	}
}

void UUEHHVCustomizationWidget::SetPreviewActor(AUEHHVCustomizationPreviewActor* InPreviewActor)
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

void UUEHHVCustomizationWidget::SetCatalog(UUEHHVCustomizationCatalog* InCatalog)
{
	Catalog = InCatalog;
	RebuildCategories();
	RebuildOptions();
}

void UUEHHVCustomizationWidget::SelectOption(EUEHHVCustomizationCategory Category, int32 Index)
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

void UUEHHVCustomizationWidget::SelectGender(EUEHHVGender Gender)
{
	if (PreviewActor)
	{
		PreviewActor->SelectGender(Gender);
		RefreshFromPreview();
	}
	RebuildOptions();
	SynchronizeControls();
}

void UUEHHVCustomizationWidget::SelectColor(EUEHHVColorChannel Channel, FLinearColor Color)
{
	if (PreviewActor)
	{
		PreviewActor->SetColor(Channel, Color);
		RefreshFromPreview();
	}
	RebuildParameterControls();
	SynchronizeControls();
}

void UUEHHVCustomizationWidget::StartWithCurrentAppearance()
{
	RefreshFromPreview();
	if (UUEGameInstance* UEGameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		UEGameInstance->SetPendingHHVAppearance(CachedAppearance);
	}

	if (!StartLevelName.IsNone())
	{
		FString LevelNameString = StartLevelName.ToString();
		if (LevelNameString == TEXT("PlayerTestLevel"))
		{
			LevelNameString = TEXT("/Game/Level/PlayerTestLevel");
		}

		// 커마 레벨에서 넘어갈 때는 맵의 기존 월드 세팅보다 우리 게임 플레이어 게임모드를 우선 사용한다.
		UGameplayStatics::OpenLevel(
			this,
			FName(*LevelNameString),
			true,
			// 맵 이동 옵션은 '?'로 시작해야 PlayerTestLevel의 기본 게임모드보다
			// 우리 플레이어 캐릭터 게임모드가 우선 적용된다.
			TEXT("?game=/Script/HeavenHyperVoice.UEGameModeBase"));
	}
}

void UUEHHVCustomizationWidget::OpenCategory(EUEHHVCustomizationCategory Category)
{
	CurrentCategory = Category;
	RebuildCategories();
	RebuildOptions();
	RebuildParameterControls();
	SynchronizeControls();
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
			return TEXT("Type 1");
		}
		if (Option.Id.Equals(TEXT("TypeB"), ESearchCase::IgnoreCase))
		{
			return TEXT("Type 2");
		}
	}
	return Option.DisplayName.IsEmpty() ? Option.Id : Option.DisplayName;
}

UTexture2D* UUEHHVCustomizationWidget::GetOptionIcon(EUEHHVCustomizationCategory Category, int32 Index) const
{
	if (!Catalog)
	{
		return nullptr;
	}

	return Catalog->GetOption(Category, Index).Icon;
}

USkeletalMesh* UUEHHVCustomizationWidget::GetOptionMesh(EUEHHVCustomizationCategory Category, int32 Index) const
{
	if (!Catalog)
	{
		return nullptr;
	}

	const EUEHHVGender Gender = PreviewActor ? PreviewActor->GetAppearance().Gender : CachedAppearance.Gender;
	return Catalog->GetOption(Category, Index).LoadMesh(Gender);
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

TSharedRef<SWidget> UUEHHVCustomizationWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("HeavenHyperVoiceCustomizationWidgetTree"));
	}
	if (!BindDesignerInterface())
	{
		UE_LOG(LogTemp, Warning, TEXT("Character customization WBP designer canvas is missing required widgets."));
	}
	return Super::RebuildWidget();
}

void UUEHHVCustomizationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("DisableAllScreenMessages"));
	RefreshFromPreview();
	BindDesignerInterface();
	RebuildCategories();
	RebuildOptions();
	SynchronizeControls();
}

void UUEHHVCustomizationWidget::HandleStartClicked()
{
	StartWithCurrentAppearance();
}


