// Fill out your copyright notice in the Description page of Project Settings.


#include "UEGameInstance.h"
#include "../System/UEAssetManager.h"
#include "CharacterSelection/UECharacterSlotSaveGame.h"

#include "Kismet/GameplayStatics.h"

const FString UUEGameInstance::CharacterSlotSaveName = TEXT("HHVCharacterSlots");

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
	return IsValidCharacterSlot(SlotIndex) && CharacterSlotSave->Slots[SlotIndex].bOccupied;
}

bool UUEGameInstance::SelectCharacterSlot(int32 SlotIndex)
{
	if (!IsValidCharacterSlot(SlotIndex))
	{
		return false;
	}

	CharacterSlotSave->SelectedSlotIndex = SlotIndex;
	if (CharacterSlotSave->Slots[SlotIndex].bOccupied)
	{
		SetPendingHHVAppearance(CharacterSlotSave->Slots[SlotIndex].Appearance);
	}
	else
	{
		ClearPendingHHVAppearance();
	}

	return SaveCharacterSlots();
}

bool UUEGameInstance::SaveAppearanceToSelectedSlot(const FUEHHVAppearance& Appearance)
{
	const int32 SlotIndex = GetSelectedCharacterSlotIndex();
	if (!IsValidCharacterSlot(SlotIndex))
	{
		return false;
	}

	FUECharacterSlotData& Slot = CharacterSlotSave->Slots[SlotIndex];
	Slot.bOccupied = true;
	Slot.Appearance = Appearance;
	SetPendingHHVAppearance(Appearance);
	return SaveCharacterSlots();
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
