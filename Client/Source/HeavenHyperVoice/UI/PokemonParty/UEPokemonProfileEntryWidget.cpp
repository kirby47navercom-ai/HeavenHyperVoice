#include "UEPokemonProfileEntryWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

UUEPokemonProfileEntryWidget::UUEPokemonProfileEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 이 값은 WBP 클래스 기본값에서 언제든 수정할 수 있는 초기 표시 형식이다.
	LevelFormat = FText::FromString(TEXT("레벨 {0}"));
	HPFormat = FText::FromString(TEXT("{0} / {1}"));

	DesignerPreviewData.SpeciesId = TEXT("Shinx");
	DesignerPreviewData.DisplayName = FText::FromString(TEXT("꼬링크"));
	DesignerPreviewData.Level = 25;
	DesignerPreviewData.CurrentHP = 78.0f;
	DesignerPreviewData.MaxHP = 100.0f;
	DesignerPreviewData.bSelected = true;
}

void UUEPokemonProfileEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너에서도 실제 값이 들어온 것처럼 보여야 팀원이 레이아웃을 바로 수정할 수 있다.
	if (IsDesignTime() && bShowDesignerPreview)
	{
		ProfileData = DesignerPreviewData;
	}

	ApplyProfileToBoundWidgets();
}

void UUEPokemonProfileEntryWidget::SetProfileData(const FUEPokemonProfileViewData& NewProfileData)
{
	ProfileData = NewProfileData;
	ApplyProfileToBoundWidgets();
	BP_OnProfileDataChanged(ProfileData);
}

void UUEPokemonProfileEntryWidget::ApplyProfileToBoundWidgets()
{
	const FText ResolvedName = ProfileData.DisplayName.IsEmpty()
		? FText::FromName(ProfileData.SpeciesId)
		: ProfileData.DisplayName;

	if (PokemonNameText)
	{
		PokemonNameText->SetText(ResolvedName);
	}

	if (LevelText)
	{
		LevelText->SetText(FText::Format(LevelFormat, FText::AsNumber(FMath::Max(ProfileData.Level, 1))));
	}

	const float SafeMaxHP = FMath::Max(ProfileData.MaxHP, 1.0f);
	const float SafeCurrentHP = FMath::Clamp(ProfileData.CurrentHP, 0.0f, SafeMaxHP);
	if (HPBar)
	{
		HPBar->SetPercent(SafeCurrentHP / SafeMaxHP);
	}
	if (HPText)
	{
		HPText->SetText(FText::Format(
			HPFormat,
			FText::AsNumber(FMath::RoundToInt(SafeCurrentHP)),
			FText::AsNumber(FMath::RoundToInt(SafeMaxHP))));
	}

	if (PokemonIcon)
	{
		if (ProfileData.ProfileIcon)
		{
			// 슬롯마다 크기가 다르므로 원본 해상도를 강제하지 않고 WBP가 정한 영역에 맞춘다.
			PokemonIcon->SetBrushFromTexture(ProfileData.ProfileIcon, false);
			PokemonIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			PokemonIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (PokemonFallbackText)
	{
		const FString NameString = ResolvedName.ToString();
		PokemonFallbackText->SetText(FText::FromString(NameString.IsEmpty() ? TEXT("?") : NameString.Left(1)));
		PokemonFallbackText->SetVisibility(ProfileData.ProfileIcon
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}

	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(ProfileData.bSelected
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}
