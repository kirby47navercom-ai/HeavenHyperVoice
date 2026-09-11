#include "UEOptionsMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "InputCoreTypes.h"
#include "../../Player/UEPlayerController.h"
#include "../../System/UEGameInstance.h"

void UUEOptionsCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshContent();
}

void UUEOptionsCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CardButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
	HoverAmount = 0.0f;
	SetRenderTranslation(FVector2D::ZeroVector);
}

void UUEOptionsCardWidget::RefreshContent()
{
	if (TitleText) TitleText->SetText(Title);
	if (IconImage) IconImage->SetBrushFromTexture(IconTexture);
}

void UUEOptionsCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const bool bHighlighted = CardButton->GetIsEnabled()
		&& (CardButton->IsHovered() || CardButton->HasKeyboardFocus());
	const float Target = bHighlighted ? 1.0f : 0.0f;
	HoverAmount = FMath::FInterpTo(HoverAmount, Target, InDeltaTime, ResponseSpeed);
	SetRenderTranslation(FVector2D(0.0f, -HoverLift * HoverAmount));
}

void UUEOptionsCardWidget::HandleClicked()
{
	if (!ActionId.IsNone())
	{
		OnActionRequested.Broadcast(ActionId);
	}
}

void UUEOptionsMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	SetIsFocusable(true);
	if (const UUEGameInstance* GI = GetGameInstance<UUEGameInstance>(); GI && PlayerNameText)
	{
		PlayerNameText->SetText(FText::FromString(GI->GetLocalSessionNickname()));
	}
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (UUEOptionsCardWidget* Card = Cast<UUEOptionsCardWidget>(Widget))
		{
			Card->OnActionRequested.AddUniqueDynamic(this, &ThisClass::HandleActionRequested);
		}
	});
	LastRequestedAction = NAME_None;
	PlayEntrance();
}

void UUEOptionsMenuWidget::NativeDestruct()
{
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (UUEOptionsCardWidget* Card = Cast<UUEOptionsCardWidget>(Widget))
		{
			Card->OnActionRequested.RemoveDynamic(this, &ThisClass::HandleActionRequested);
		}
	});
	CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	Super::NativeDestruct();
}

void UUEOptionsMenuWidget::PlayEntrance()
{
	EntranceElapsed = 0.0f;
	ApplyEntrance(bAnimateEntrance && EntranceDuration > 0.0f ? 0.0f : 1.0f);
}

void UUEOptionsMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bAnimateEntrance && EntranceDuration > 0.0f && EntranceElapsed < EntranceDuration)
	{
		EntranceElapsed = FMath::Min(EntranceElapsed + InDeltaTime, EntranceDuration);
		ApplyEntrance(EntranceElapsed / EntranceDuration);
	}
}

void UUEOptionsMenuWidget::ApplyEntrance(float Progress)
{
	if (!MenuSurface) return;
	const float Eased = 1.0f - FMath::Pow(1.0f - FMath::Clamp(Progress, 0.0f, 1.0f), 3.0f);
	MenuSurface->SetRenderOpacity(1.0f);
	MenuSurface->SetRenderTranslation(FVector2D(-EntranceOffset * (1.0f - Eased), 0.0f));
}

void UUEOptionsMenuWidget::HandleActionRequested(FName ActionId)
{
	LastRequestedAction = ActionId;
	OnMenuActionRequested.Broadcast(ActionId);
}

void UUEOptionsMenuWidget::HandleCloseClicked()
{
	HandleActionRequested(TEXT("Resume"));
}

FReply UUEOptionsMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (!InKeyEvent.IsRepeat()) HandleCloseClicked();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UUEOptionsHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	MenuButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenMenu);
}

void UUEOptionsHUDWidget::OpenMenu()
{
	if (AUEPlayerController* Controller = Cast<AUEPlayerController>(GetOwningPlayer()))
	{
		Controller->ToggleOptionsMenu();
	}
}
