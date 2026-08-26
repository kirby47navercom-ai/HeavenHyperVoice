#include "UEPokemonPartyWidget.h"

#include "../../Character/UEPlayerCharacter.h"
#include "../../Pokemon/Server/UEPokemonServerSubsystem.h"
#include "../../Pokemon/UEPokemonSpeciesData.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

UUEPokemonPartyWidget::UUEPokemonPartyWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UnknownPokemonText = FText::FromString(TEXT("알 수 없는 포켓몬"));
	ActiveLevelFormat = FText::FromString(TEXT("레벨 {0}"));
	ActiveHPFormat = FText::FromString(TEXT("{0} / {1}"));
}

void UUEPokemonPartyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!SourcePlayer.IsValid())
	{
		SourcePlayer = Cast<AUEPlayerCharacter>(GetOwningPlayerPawn());
	}

	BindRosterDelegate();
	RefreshProfiles();
}

void UUEPokemonPartyWidget::NativeDestruct()
{
	if (UUEPokemonServerSubsystem* ServerSubsystem = GetPokemonServerSubsystem())
	{
		ServerSubsystem->OnOwnedPokemonRosterChanged.RemoveDynamic(
			this,
			&UUEPokemonPartyWidget::HandleOwnedRosterChanged);
	}

	Super::NativeDestruct();
}

void UUEPokemonPartyWidget::InitializeForPlayer(AUEPlayerCharacter* PlayerCharacter)
{
	SourcePlayer = PlayerCharacter;
	BindRosterDelegate();
	RefreshProfiles();
}

void UUEPokemonPartyWidget::BindRosterDelegate()
{
	if (UUEPokemonServerSubsystem* ServerSubsystem = GetPokemonServerSubsystem())
	{
		ServerSubsystem->OnOwnedPokemonRosterChanged.AddUniqueDynamic(
			this,
			&UUEPokemonPartyWidget::HandleOwnedRosterChanged);
	}
}

void UUEPokemonPartyWidget::HandleOwnedRosterChanged(int32 OwnerServerPlayerId)
{
	if (SourcePlayer.IsValid() && SourcePlayer->GetPokemonServerPlayerId() == OwnerServerPlayerId)
	{
		RefreshProfiles();
	}
}

void UUEPokemonPartyWidget::RefreshProfiles()
{
	Profiles.Reset();

	if (!SourcePlayer.IsValid())
	{
		SourcePlayer = Cast<AUEPlayerCharacter>(GetOwningPlayerPawn());
	}

	UUEPokemonServerSubsystem* ServerSubsystem = GetPokemonServerSubsystem();
	if (!SourcePlayer.IsValid() || !ServerSubsystem)
	{
		return;
	}

	const int32 OwnerServerPlayerId = SourcePlayer->GetPokemonServerPlayerId();
	const int32 SelectedPokemonInstanceId = SourcePlayer->GetSelectedPokemonCompanionInstanceId();
	const TArray<FUEPokemonServerOwnedPokemon> OwnedPokemons =
		ServerSubsystem->GetOwnedPokemons(OwnerServerPlayerId);

	for (const FUEPokemonServerOwnedPokemon& OwnedPokemon : OwnedPokemons)
	{
		FUEPokemonProfileViewData Profile;
		Profile.PokemonInstanceId = OwnedPokemon.PokemonInstanceId;
		Profile.SpeciesId = OwnedPokemon.SpeciesId;
		Profile.Level = FMath::Max(OwnedPokemon.Level, 1);
		Profile.CurrentHP = FMath::Max(OwnedPokemon.CurrentHP, 0.0f);
		Profile.bSelected = OwnedPokemon.PokemonInstanceId == SelectedPokemonInstanceId;
		Profile.bCanSummon = OwnedPokemon.bCanSummon;

		if (OwnedPokemon.SpeciesData)
		{
			Profile.DisplayName = OwnedPokemon.SpeciesData->DisplayName.IsEmpty()
				? FText::FromName(OwnedPokemon.SpeciesData->SpeciesId)
				: OwnedPokemon.SpeciesData->DisplayName;
			Profile.ProfileIcon = OwnedPokemon.SpeciesData->ProfileIcon;
			Profile.MaxHP = FMath::Max(OwnedPokemon.SpeciesData->MaxHP, 1.0f);
		}
		else
		{
			Profile.DisplayName = OwnedPokemon.SpeciesId.IsNone()
				? UnknownPokemonText
				: FText::FromName(OwnedPokemon.SpeciesId);
			Profile.MaxHP = FMath::Max(OwnedPokemon.CurrentHP, 1.0f);
		}

		Profile.CurrentHP = FMath::Clamp(Profile.CurrentHP, 0.0f, Profile.MaxHP);
		Profiles.Add(Profile);
	}

	// 선택된 포켓몬을 크게 보여주되, 아직 선택 정보가 없으면 첫 번째 보유 포켓몬을 대표로 사용한다.
	const FUEPokemonProfileViewData* ActiveProfile = Profiles.FindByPredicate(
		[](const FUEPokemonProfileViewData& Profile)
		{
			return Profile.bSelected;
		});
	if (!ActiveProfile && !Profiles.IsEmpty())
	{
		ActiveProfile = &Profiles[0];
	}
	ApplyActiveProfile(ActiveProfile);

	if (ProfileList)
	{
		ProfileList->ClearChildren();
		if (ProfileEntryWidgetClass && GetOwningPlayer())
		{
			for (const FUEPokemonProfileViewData& Profile : Profiles)
			{
				UUEPokemonProfileEntryWidget* EntryWidget =
					CreateWidget<UUEPokemonProfileEntryWidget>(GetOwningPlayer(), ProfileEntryWidgetClass);
				if (EntryWidget)
				{
					EntryWidget->SetProfileData(Profile);
					ProfileList->AddChild(EntryWidget);
				}
			}
		}
	}

	BP_OnPartyProfilesRebuilt(Profiles);
}

void UUEPokemonPartyWidget::ApplyActiveProfile(const FUEPokemonProfileViewData* ActiveProfile)
{
	if (ActiveProfilePanel)
	{
		ActiveProfilePanel->SetVisibility(ActiveProfile
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (!ActiveProfile)
	{
		return;
	}

	const FText ResolvedName = ActiveProfile->DisplayName.IsEmpty()
		? FText::FromName(ActiveProfile->SpeciesId)
		: ActiveProfile->DisplayName;
	if (ActivePokemonNameText)
	{
		ActivePokemonNameText->SetText(ResolvedName);
	}
	if (ActivePokemonLevelText)
	{
		ActivePokemonLevelText->SetText(FText::Format(
			ActiveLevelFormat,
			FText::AsNumber(FMath::Max(ActiveProfile->Level, 1))));
	}

	const float SafeMaxHP = FMath::Max(ActiveProfile->MaxHP, 1.0f);
	const float SafeCurrentHP = FMath::Clamp(ActiveProfile->CurrentHP, 0.0f, SafeMaxHP);
	if (ActivePokemonHPBar)
	{
		ActivePokemonHPBar->SetPercent(SafeCurrentHP / SafeMaxHP);
	}
	if (ActivePokemonHPText)
	{
		ActivePokemonHPText->SetText(FText::Format(
			ActiveHPFormat,
			FText::AsNumber(FMath::RoundToInt(SafeCurrentHP)),
			FText::AsNumber(FMath::RoundToInt(SafeMaxHP))));
	}

	if (ActivePokemonIcon)
	{
		if (ActiveProfile->ProfileIcon)
		{
			// 원본 텍스처 해상도로 위젯 크기를 덮지 않는다. 실제 얼굴 크기는 WBP의 슬롯에서 수정한다.
			ActivePokemonIcon->SetBrushFromTexture(ActiveProfile->ProfileIcon, false);
			ActivePokemonIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ActivePokemonIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (ActivePokemonFallbackText)
	{
		const FString NameString = ResolvedName.ToString();
		ActivePokemonFallbackText->SetText(FText::FromString(
			NameString.IsEmpty() ? TEXT("?") : NameString.Left(1)));
		ActivePokemonFallbackText->SetVisibility(ActiveProfile->ProfileIcon
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}
}

UUEPokemonServerSubsystem* UUEPokemonPartyWidget::GetPokemonServerSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UUEPokemonServerSubsystem>() : nullptr;
}
