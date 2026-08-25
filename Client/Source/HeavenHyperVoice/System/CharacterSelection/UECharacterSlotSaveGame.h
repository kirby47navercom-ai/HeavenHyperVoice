#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "../../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"
#include "UECharacterSlotSaveGame.generated.h"

class UUEPokemonSpeciesData;

USTRUCT(BlueprintType)
struct HEAVENHYPERVOICE_API FUECharacterSlotData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Character Slot")
	bool bOccupied = false;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Character Slot")
	FUEHHVAppearance Appearance;

	// 로비와 실제 플레이에서 함께 사용할 캐릭터 이름이다.
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Character Slot")
	FString CharacterName;

	// 스타팅 포켓몬은 에셋 경로를 코드에 넣지 않고 블루프린트에서 고른 데이터 에셋으로 저장한다.
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Character Slot")
	TSoftObjectPtr<UUEPokemonSpeciesData> PartnerSpecies;
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
