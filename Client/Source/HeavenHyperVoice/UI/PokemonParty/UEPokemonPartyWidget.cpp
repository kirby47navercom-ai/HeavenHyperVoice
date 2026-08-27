#include "UEPokemonPartyWidget.h"

#include "../../Character/UEPlayerCharacter.h"
#include "../../Server/UEFieldClientSubsystem.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

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
	if (UUEFieldClientSubsystem* FieldClientSubsystem = GetFieldClientSubsystem())
	{
		FieldClientSubsystem->OnPokemonPartyChanged.RemoveDynamic(
			this,
			&UUEPokemonPartyWidget::HandlePokemonPartyChanged);
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
	if (UUEFieldClientSubsystem* FieldClientSubsystem = GetFieldClientSubsystem())
	{
		FieldClientSubsystem->OnPokemonPartyChanged.AddUniqueDynamic(
			this,
			&UUEPokemonPartyWidget::HandlePokemonPartyChanged);
	}
}

void UUEPokemonPartyWidget::HandlePokemonPartyChanged()
{
	RefreshProfiles();
}

void UUEPokemonPartyWidget::RefreshProfiles()
{
	Profiles.Reset();

	if (!SourcePlayer.IsValid())
	{
		SourcePlayer = Cast<AUEPlayerCharacter>(GetOwningPlayerPawn());
	}

	UUEFieldClientSubsystem* FieldClientSubsystem = GetFieldClientSubsystem();
	if (!FieldClientSubsystem)
	{
		return;
	}

	const TArray<FUEFieldPokemonPartyEntry>& ServerProfiles = FieldClientSubsystem->GetCachedPokemonPartyEntries();

	for (const FUEFieldPokemonPartyEntry& ServerProfile : ServerProfiles)
	{
		FUEPokemonProfileViewData Profile;
		Profile.PokemonInstanceId = ServerProfile.PokemonInstanceId;
		Profile.SpeciesId = ServerProfile.SpeciesId;
		Profile.DisplayName = ServerProfile.DisplayName.IsEmpty()
			? (ServerProfile.SpeciesId.IsNone() ? UnknownPokemonText : FText::FromName(ServerProfile.SpeciesId))
			: ServerProfile.DisplayName;
		Profile.ProfileIcon = ServerProfile.ProfileIcon;
		Profile.Level = FMath::Max(ServerProfile.Level, 1);
		Profile.CurrentHP = FMath::Max(ServerProfile.CurrentHP, 0.0f);
		Profile.MaxHP = FMath::Max(ServerProfile.MaxHP, 1.0f);
		Profile.bSelected = ServerProfile.bSelected;
		Profile.bCanSummon = ServerProfile.bCanSummon;

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

UUEFieldClientSubsystem* UUEPokemonPartyWidget::GetFieldClientSubsystem() const
{
	if (const APlayerController* OwningPlayer = GetOwningPlayer())
	{
		if (ULocalPlayer* LocalPlayer = OwningPlayer->GetLocalPlayer())
		{
			return LocalPlayer->GetSubsystem<UUEFieldClientSubsystem>();
		}
	}

	return SourcePlayer.IsValid()
		? UUEFieldClientSubsystem::Get(SourcePlayer.Get())
		: nullptr;
}
