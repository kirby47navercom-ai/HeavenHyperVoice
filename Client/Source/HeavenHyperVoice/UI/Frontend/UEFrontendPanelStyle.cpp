#include "UEFrontendPanelStyle.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

void UEFrontendStatus::Apply(
	const TMap<EUEFrontendStatusKind, FUEFrontendStatusStyle>& Styles,
	EUEFrontendStatusKind Kind,
	UTextBlock* Text,
	UImage* Icon,
	UWidget* Wave)
{
	const FUEFrontendStatusStyle* Style = Styles.Find(Kind);
	if (!Style)
	{
		return;
	}

	if (Text)
	{
		Text->SetColorAndOpacity(FSlateColor(Style->Color));
	}
	if (Icon)
	{
		if (Style->Icon && !Style->bShowWave)
		{
			Icon->SetBrushFromTexture(Style->Icon);
			Icon->SetColorAndOpacity(Style->Color);
			Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// Hidden 이라 자리는 남는다. 문구가 좌우로 흔들리지 않는다.
			Icon->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	if (Wave)
	{
		Wave->SetVisibility(Style->bShowWave ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

namespace
{
const FString RingSuffix = TEXT("FocusRing");

bool ShowsNavigationFocus(UWidget* Target)
{
	const TSharedPtr<SWidget> SlateWidget = Target->GetCachedWidget();
	if (!SlateWidget || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	bool bShow = false;
	FSlateApplication::Get().ForEachUser([&SlateWidget, &bShow](FSlateUser& User)
	{
		bShow |= User.ShouldShowFocus(SlateWidget);
	});
	return bShow;
}
}

void FUEFrontendFocusRings::Collect(UUserWidget* Owner)
{
	Rings.Reset();
	if (!Owner || !Owner->WidgetTree)
	{
		return;
	}

	Owner->WidgetTree->ForEachWidget([this, Owner](UWidget* Widget)
	{
		const FString Name = Widget->GetName();
		if (!Name.EndsWith(RingSuffix))
		{
			return;
		}
		UWidget* Target = Owner->WidgetTree->FindWidget(FName(Name.LeftChop(RingSuffix.Len())));
		if (!Target)
		{
			return;
		}
		Widget->SetVisibility(ESlateVisibility::Hidden);
		Rings.Add({Target, Widget});
	});
}

void FUEFrontendFocusRings::Refresh()
{
	for (const FRing& Entry : Rings)
	{
		UWidget* Target = Entry.Target.Get();
		UWidget* Ring = Entry.Ring.Get();
		if (!Target || !Ring)
		{
			continue;
		}

		const ESlateVisibility Wanted = ShowsNavigationFocus(Target)
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden;
		if (Ring->GetVisibility() != Wanted)
		{
			Ring->SetVisibility(Wanted);
		}
	}
}
