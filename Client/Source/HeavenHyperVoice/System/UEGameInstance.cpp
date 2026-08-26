// Fill out your copyright notice in the Description page of Project Settings.


#include "UEGameInstance.h"
#include "../System/UEAssetManager.h"
#include "CharacterSelection/UECharacterSlotSaveGame.h"
#include "../Pokemon/UEPokemonSpeciesData.h"

#include "Kismet/GameplayStatics.h"

UUEGameInstance::UUEGameInstance(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UUEGameInstance::Init()
{
	Super::Init();

	UUEAssetManager::Initialize();
	LoadCharacterSlots();
}

void UUEGameInstance::Shutdown()
{
	Super::Shutdown();

}

void UUEGameInstance::SetPendingHHVAppearance(const FUEHHVAppearance& NewAppearance)
{
	PendingHHVAppearance = NewAppearance;
	bHasPendingHHVAppearance = true;
}

bool UUEGameInstance::GetPendingHHVAppearance(FUEHHVAppearance& OutAppearance) const
{
	if (!bHasPendingHHVAppearance)
	{
		return false;
	}

	OutAppearance = PendingHHVAppearance;
	return true;
}

void UUEGameInstance::ClearPendingHHVAppearance()
{
	bHasPendingHHVAppearance = false;
	PendingHHVAppearance = FUEHHVAppearance();
}

bool UUEGameInstance::GetCharacterSlot(int32 SlotIndex, FUECharacterSlotData& OutSlot) const
{
	if (!IsValidCharacterSlot(SlotIndex))
	{
		return false;
	}

	OutSlot = CharacterSlotSave->Slots[SlotIndex];
	return true;
}

bool UUEGameInstance::IsCharacterSlotOccupied(int32 SlotIndex) const
{
	if (!IsValidCharacterSlot(SlotIndex))
	{
		return false;
	}

	const FUECharacterSlotData& Slot = CharacterSlotSave->Slots[SlotIndex];
	// 이름·외형·파트너가 모두 확정된 슬롯만 로비의 완성 캐릭터로 취급한다.
	return Slot.bOccupied
		&& !Slot.CharacterName.TrimStartAndEnd().IsEmpty()
		&& !Slot.PartnerSpecies.IsNull();
}

bool UUEGameInstance::SelectCharacterSlot(int32 SlotIndex)
{
	if (!IsValidCharacterSlot(SlotIndex))
	{
		return false;
	}

	const int32 PreviousSelectedSlotIndex = CharacterSlotSave->SelectedSlotIndex;
	CharacterSlotSave->SelectedSlotIndex = SlotIndex;
	if (!SaveCharacterSlots())
	{
		// 저장 실패 시 메모리의 선택 상태도 이전 값으로 되돌린다.
		CharacterSlotSave->SelectedSlotIndex = PreviousSelectedSlotIndex;
		return false;
	}

	if (IsCharacterSlotOccupied(SlotIndex))
	{
		SetPendingHHVAppearance(CharacterSlotSave->Slots[SlotIndex].Appearance);
	}
	else
	{
		ClearPendingCharacterCreation();
	}
	return true;
}

bool UUEGameInstance::SaveAppearanceToSelectedSlot(const FUEHHVAppearance& Appearance)
{
	const int32 SlotIndex = GetSelectedCharacterSlotIndex();
	if (!IsValidCharacterSlot(SlotIndex))
	{
		return false;
	}

	FUECharacterSlotData& Slot = CharacterSlotSave->Slots[SlotIndex];
	const FUECharacterSlotData PreviousSlot = Slot;
	Slot.Appearance = Appearance;
	if (!SaveCharacterSlots())
	{
		// 디스크 저장이 실패하면 아직 확정되지 않은 슬롯 변경을 남기지 않는다.
		Slot = PreviousSlot;
		return false;
	}

	SetPendingHHVAppearance(Appearance);
	return true;
}

bool UUEGameInstance::SaveCharacterCreationToSelectedSlot(
	const FString& CharacterName,
	const FUEHHVAppearance& Appearance,
	UUEPokemonSpeciesData* PartnerSpecies)
{
	const int32 SlotIndex = GetSelectedCharacterSlotIndex();
	if (!IsValidCharacterSlot(SlotIndex) || CharacterName.TrimStartAndEnd().IsEmpty() || !PartnerSpecies)
	{
		return false;
	}

	FUECharacterSlotData& Slot = CharacterSlotSave->Slots[SlotIndex];
	const FUECharacterSlotData PreviousSlot = Slot;
	Slot.bOccupied = true;
	Slot.CharacterName = CharacterName.TrimStartAndEnd();
	Slot.Appearance = Appearance;
	Slot.PartnerSpecies = PartnerSpecies;
	if (!SaveCharacterSlots())
	{
		// 실패한 생성 결과는 슬롯에 남기지 않고 같은 입력으로 다시 시도할 수 있게 한다.
		Slot = PreviousSlot;
		return false;
	}

	SetPendingHHVAppearance(Appearance);
	PendingCharacterName.Reset();
	return true;
}

bool UUEGameInstance::DeleteCharacterSlot(int32 SlotIndex)
{
	if (!IsCharacterSlotOccupied(SlotIndex))
	{
		return false;
	}

	const FUECharacterSlotData PreviousSlot = CharacterSlotSave->Slots[SlotIndex];
	const int32 PreviousSelectedSlotIndex = CharacterSlotSave->SelectedSlotIndex;
	CharacterSlotSave->Slots[SlotIndex] = FUECharacterSlotData();
	if (PreviousSelectedSlotIndex == SlotIndex)
	{
		CharacterSlotSave->SelectedSlotIndex = INDEX_NONE;
	}

	if (!SaveCharacterSlots())
	{
		// 삭제 저장이 실패하면 메모리에서도 슬롯을 복구해 재시작 전후 상태를 맞춘다.
		CharacterSlotSave->Slots[SlotIndex] = PreviousSlot;
		CharacterSlotSave->SelectedSlotIndex = PreviousSelectedSlotIndex;
		return false;
	}

	if (PreviousSelectedSlotIndex == SlotIndex)
	{
		ClearPendingCharacterCreation();
	}
	return true;
}

bool UUEGameInstance::LoadSelectedSlotAppearance()
{
	const int32 SlotIndex = GetSelectedCharacterSlotIndex();
	if (!IsCharacterSlotOccupied(SlotIndex))
	{
		return false;
	}

	SetPendingHHVAppearance(CharacterSlotSave->Slots[SlotIndex].Appearance);
	return true;
}

int32 UUEGameInstance::GetSelectedCharacterSlotIndex() const
{
	return CharacterSlotSave ? CharacterSlotSave->SelectedSlotIndex : INDEX_NONE;
}

UUEPokemonSpeciesData* UUEGameInstance::GetSelectedPartnerSpecies() const
{
	FUECharacterSlotData Slot;
	if (!GetCharacterSlot(GetSelectedCharacterSlotIndex(), Slot)
		|| !IsCharacterSlotOccupied(GetSelectedCharacterSlotIndex()))
	{
		return nullptr;
	}

	return Slot.PartnerSpecies.LoadSynchronous();
}

void UUEGameInstance::SetPendingCharacterName(const FString& CharacterName)
{
	PendingCharacterName = CharacterName.TrimStartAndEnd();
}

bool UUEGameInstance::GetPendingCharacterName(FString& OutCharacterName) const
{
	OutCharacterName = PendingCharacterName;
	return !PendingCharacterName.IsEmpty();
}

void UUEGameInstance::ClearPendingCharacterCreation()
{
	PendingCharacterName.Reset();
	ClearPendingHHVAppearance();
}

void UUEGameInstance::SetLocalSession(const FString& UserId, const FString& Nickname)
{
	LocalSessionUserId = UserId.TrimStartAndEnd();
	LocalSessionNickname = Nickname.TrimStartAndEnd();
	bHasLocalSession = !LocalSessionUserId.IsEmpty();
}

void UUEGameInstance::ClearLocalSession()
{
	LocalSessionUserId.Reset();
	LocalSessionNickname.Reset();
	bHasLocalSession = false;
}

void UUEGameInstance::SetServerAddress(const FString& ServerAddress)
{
	SelectedServerAddress = ServerAddress.TrimStartAndEnd();
}

void UUEGameInstance::LoadCharacterSlots()
{
	CharacterSlotSave = Cast<UUECharacterSlotSaveGame>(
		UGameplayStatics::LoadGameFromSlot(CharacterSlotSaveName, CharacterSlotUserIndex));

	if (!CharacterSlotSave)
	{
		CharacterSlotSave = Cast<UUECharacterSlotSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UUECharacterSlotSaveGame::StaticClass()));
	}

	if (!CharacterSlotSave)
	{
		return;
	}

	CharacterSlotSave->Slots.SetNum(CharacterSlotCount);
	if (!IsValidCharacterSlot(CharacterSlotSave->SelectedSlotIndex))
	{
		CharacterSlotSave->SelectedSlotIndex = INDEX_NONE;
	}

	LoadSelectedSlotAppearance();
}

bool UUEGameInstance::SaveCharacterSlots() const
{
	return CharacterSlotSave && UGameplayStatics::SaveGameToSlot(
		CharacterSlotSave,
		CharacterSlotSaveName,
		CharacterSlotUserIndex);
}

bool UUEGameInstance::IsValidCharacterSlot(int32 SlotIndex) const
{
	return CharacterSlotSave && CharacterSlotSave->Slots.IsValidIndex(SlotIndex);
}
