#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "../../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"
#include "UECharacterSlotSaveGame.generated.h"

USTRUCT(BlueprintType)
struct HEAVENHYPERVOICE_API FUECharacterSlotData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Character Slot")
	bool bOccupied = false;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Character Slot")
	FUEHHVAppearance Appearance;
};

/** 세 개의 로컬 캐릭터 슬롯과 마지막 선택 슬롯을 보관한다. */
UCLASS()
class HEAVENHYPERVOICE_API UUECharacterSlotSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	TArray<FUECharacterSlotData> Slots;

	UPROPERTY(SaveGame)
	int32 SelectedSlotIndex = INDEX_NONE;
};
