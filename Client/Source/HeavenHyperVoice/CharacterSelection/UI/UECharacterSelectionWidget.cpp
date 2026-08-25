#include "UECharacterSelectionWidget.h"

#include "../../System/UEGameInstance.h"
#include "../../Pokemon/UEPokemonSpeciesData.h"

#include "Components/TextBlock.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

void UUECharacterSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const TArray<UUECharacterLobbySlotWidget*> LobbySlots = GetLobbySlots();
	for (int32 SlotIndex = 0; SlotIndex < LobbySlots.Num(); ++SlotIndex)
	{
		LobbySlots[SlotIndex]->InitializeSlot(SlotIndex);
		LobbySlots[SlotIndex]->OnActionRequested.AddUniqueDynamic(
			this,
			&ThisClass::HandleLobbySlotAction);
		LobbySlots[SlotIndex]->OnDeleteRequested.AddUniqueDynamic(
			this,
			&ThisClass::HandleLobbySlotDelete);
	}

	const UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	const FString SessionNickname = GameInstance ? GameInstance->GetLocalSessionNickname() : FString();
	SetAccountName(SessionNickname.IsEmpty() ? DefaultAccountName : FText::FromString(SessionNickname));
	RefreshLobby();
}

void UUECharacterSelectionWidget::NativeDestruct()
{
	for (UUECharacterLobbySlotWidget* LobbySlot : GetLobbySlots())
	{
		if (!LobbySlot)
		{
			continue;
		}
		LobbySlot->OnActionRequested.RemoveDynamic(this, &ThisClass::HandleLobbySlotAction);
		LobbySlot->OnDeleteRequested.RemoveDynamic(this, &ThisClass::HandleLobbySlotDelete);
	}
	Super::NativeDestruct();
}

void UUECharacterSelectionWidget::RefreshLobby()
{
	UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		return;
	}

	const TArray<UUECharacterLobbySlotWidget*> LobbySlots = GetLobbySlots();
	int32 OccupiedCount = 0;
	int32 FirstAvailableSlot = INDEX_NONE;
	for (int32 SlotIndex = 0; SlotIndex < LobbySlots.Num(); ++SlotIndex)
	{
		if (GameInstance->IsCharacterSlotOccupied(SlotIndex))
		{
			++OccupiedCount;
		}
		else if (FirstAvailableSlot == INDEX_NONE)
		{
			FirstAvailableSlot = SlotIndex;
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < LobbySlots.Num(); ++SlotIndex)
	{
		FUECharacterLobbySlotViewData ViewData;
		if (SlotDisplayDefaults.IsValidIndex(SlotIndex))
		{
			ViewData = SlotDisplayDefaults[SlotIndex];
		}

		FUECharacterSlotData SavedSlot;
		if (GameInstance->GetCharacterSlot(SlotIndex, SavedSlot)
			&& GameInstance->IsCharacterSlotOccupied(SlotIndex))
		{
			ViewData.State = EUECharacterLobbySlotState::Occupied;
			ViewData.Appearance = SavedSlot.Appearance;
			if (!SavedSlot.CharacterName.IsEmpty())
			{
				ViewData.CharacterName = FText::FromString(SavedSlot.CharacterName);
			}

			if (UUEPokemonSpeciesData* PartnerSpecies = SavedSlot.PartnerSpecies.LoadSynchronous())
			{
				ViewData.PartnerSpecies = PartnerSpecies;
				FString PartnerDisplayName = PartnerSpecies->GetName();
				PartnerDisplayName.RemoveFromStart(TEXT("DA_"));
				ViewData.PartnerName = FText::FromString(PartnerDisplayName);
			}
		}
		else
		{
			ViewData.State = SlotIndex == FirstAvailableSlot
				? EUECharacterLobbySlotState::Available
				: EUECharacterLobbySlotState::Locked;
		}

		LobbySlots[SlotIndex]->ApplyViewData(ViewData);
	}

	OccupiedCountValueText->SetText(FText::AsNumber(OccupiedCount));
	TotalCountValueText->SetText(FText::AsNumber(LobbySlots.Num()));
}

void UUECharacterSelectionWidget::SetAccountName(const FText& InAccountName)
{
	AccountNameText->SetText(InAccountName);
}

void UUECharacterSelectionWidget::SelectSlot(int32 SlotIndex)
{
	HandleLobbySlotAction(SlotIndex);
}

void UUECharacterSelectionWidget::StartSelectedCharacter()
{
	EnterSelectedCharacter();
}

void UUECharacterSelectionWidget::RefreshSlots()
{
	RefreshLobby();
}

void UUECharacterSelectionWidget::HandleLobbySlotAction(int32 SlotIndex)
{
	UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!GameInstance || !GameInstance->SelectCharacterSlot(SlotIndex))
	{
		return;
	}

	if (GameInstance->IsCharacterSlotOccupied(SlotIndex))
	{
		EnterSelectedCharacter();
		return;
	}

	OnCharacterCreationRequested.Broadcast(SlotIndex);
}

void UUECharacterSelectionWidget::HandleLobbySlotDelete(int32 SlotIndex)
{
	if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		if (GameInstance->DeleteCharacterSlot(SlotIndex))
		{
			RefreshLobby();
		}
	}
}

void UUECharacterSelectionWidget::EnterSelectedCharacter()
{
	UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!GameInstance || !GameInstance->LoadSelectedSlotAppearance())
	{
		if (StatusText)
		{
			StatusText->SetText(NoCharacterSelectedMessage);
		}
		return;
	}

	if (GameplayLevel.IsNull() || !GameplayGameModeClass)
	{
		UE_LOG(LogTemp, Error, TEXT("게임 레벨 또는 게임 모드가 WBP_CharacterSelection 기본값에 지정되지 않았습니다."));
		return;
	}

	// 맵 경로를 문자열로 만들지 않고 블루프린트가 참조한 소프트 오브젝트를 사용한다.
	const FString TravelOptions = FString::Printf(TEXT("?game=%s"), *GameplayGameModeClass->GetPathName());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, GameplayLevel, true, TravelOptions);
}

TArray<UUECharacterLobbySlotWidget*> UUECharacterSelectionWidget::GetLobbySlots() const
{
	return {LobbySlot0, LobbySlot1, LobbySlot2};
}
