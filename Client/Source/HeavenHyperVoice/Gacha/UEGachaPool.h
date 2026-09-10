#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UEGachaPool.generated.h"

class UUEPokemonSpeciesData;

UENUM(BlueprintType)
enum class EUEGachaRarity : uint8
{
	Normal UMETA(DisplayName="일반 · 몬스터볼"),
	Rare UMETA(DisplayName="레어 · 슈퍼볼"),
	SuperRare UMETA(DisplayName="슈퍼레어 · 하이퍼볼")
};

USTRUCT(BlueprintType)
struct FUEGachaEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UUEPokemonSpeciesData> Species;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 DexNumber = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EUEGachaRarity Rarity = EUEGachaRarity::Normal;
	/** 이 기계 안에서 사용하는 상대 가중치다. 0이면 추첨 대상에서 제외한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float Weight = 1.0f;
};

UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEGachaPool : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText MachineName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor TypeColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 DisplayOrder = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText NormalLabel = NSLOCTEXT("Gacha", "Normal", "일반");
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText RareLabel = NSLOCTEXT("Gacha", "Rare", "레어");
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText SuperRareLabel = NSLOCTEXT("Gacha", "SuperRare", "슈퍼레어");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(TitleProperty="DisplayName")) TArray<FUEGachaEntry> Entries;
	UFUNCTION(BlueprintPure) FText GetRarityLabel(EUEGachaRarity Rarity) const;
	UFUNCTION(BlueprintPure) float GetEntryProbability(int32 Index) const;
	bool Draw(FUEGachaEntry& Result) const;
};
