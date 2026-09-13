#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UEGachaPool.generated.h"

class UUEPokemonSpeciesData;

/**
 * 서버에 보낼 뽑기 종류.
 *
 * field.fbs 의 GachaType, Protocol/GachaPool.h 의 proto::GachaType 과 값이
 * 같아야 한다. None 은 "안 정했다" 이고 서버가 거절한다 — 기본값이 불꽃이
 * 되면 타입을 빠뜨린 기계가 조용히 불꽃을 뽑는다.
 */
UENUM(BlueprintType)
enum class EUEGachaType : uint8
{
	None = 0 UMETA(DisplayName="(설정 안 함)"),
	Fire = 1 UMETA(DisplayName="불꽃"),
	Water = 2 UMETA(DisplayName="물"),
	Grass = 3 UMETA(DisplayName="풀"),
	Normal = 4 UMETA(DisplayName="노말"),
	Electric = 5 UMETA(DisplayName="전기")
};

UENUM(BlueprintType)
enum class EUEGachaRarity : uint8
{
	Normal UMETA(DisplayName="일반 · 몬스터볼"),
	Rare UMETA(DisplayName="레어 · 슈퍼볼"),
	SuperRare UMETA(DisplayName="슈퍼레어 · 하이퍼볼")
};

/** 서버가 확정한 뽑기 결과. 클라는 타입만 보내고 무엇이 나올지는 서버가 정한다. */
USTRUCT(BlueprintType)
struct FUEFieldGachaResult
{
	GENERATED_BODY()

	// 토큰이 소모됐는가. false 면 아무 일도 일어나지 않았다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Server|Gacha")
	bool bOk = false;

	// 화면에 그대로 띄울 안내.
	UPROPERTY(BlueprintReadOnly, Category = "Field Server|Gacha")
	FString Message;

	// 나온 종족의 도감번호. 실패하면 0 이다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Server|Gacha")
	int32 Dex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Field Server|Gacha")
	EUEGachaRarity Rarity = EUEGachaRarity::Normal;

	// 이미 갖고 있던 종족이다. 꽝이며 토큰은 돌려주지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Server|Gacha")
	bool bDuplicate = false;
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

	/**
	 * 서버에 보낼 종류. 비워 두면 DisplayOrder 에서 끌어낸다 (GetServerType).
	 * 다른 값을 쓰려면 여기서 명시한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EUEGachaType ServerType = EUEGachaType::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor TypeColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 DisplayOrder = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText NormalLabel = NSLOCTEXT("Gacha", "Normal", "일반");
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText RareLabel = NSLOCTEXT("Gacha", "Rare", "레어");
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText SuperRareLabel = NSLOCTEXT("Gacha", "SuperRare", "슈퍼레어");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(TitleProperty="DisplayName")) TArray<FUEGachaEntry> Entries;
	UFUNCTION(BlueprintPure) FText GetRarityLabel(EUEGachaRarity Rarity) const;
	UFUNCTION(BlueprintPure) float GetEntryProbability(int32 Index) const;

	/**
	 * 도감번호로 후보를 찾는다. 서버가 알려준 결과를 화면에 그릴 때 쓴다
	 * (표시 이름·초상화·등급 라벨이 여기 있다). 없으면 nullptr.
	 */
	const FUEGachaEntry* FindByDex(int32 Dex) const;

	/**
	 * 서버에 보낼 종류. ServerType 이 설정돼 있으면 그것을, 아니면
	 * DisplayOrder 에서 끌어낸다.
	 *
	 * 폴백이 있는 이유는 기존 DA 다섯 개를 손으로 고치지 않아도 되게 하려는
	 * 것이다. HHVGachaBuild 가 DisplayOrder 를 불꽃0·물1·풀2·노말3·전기4 로
	 * 넣었고 EUEGachaType 이 1 부터라 +1 이 그대로 맞는다.
	 */
	UFUNCTION(BlueprintPure) EUEGachaType GetServerType() const;
};
