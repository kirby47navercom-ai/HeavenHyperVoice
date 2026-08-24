#pragma once

// 종족 id -> 그 종족의 비주얼 데이터.
//
// 서버는 종족을 번호로만 보낸다 (field.fbs 의 EntityState.species). 그 번호로
// 메시·캡슐·애니메이션을 찾을 표가 지금까지 없어서, 야생 포켓몬은 전부 색만
// 다른 큐브로 떴다. 이게 그 표다.
//
// 번호는 Server/Protocol/PokemonSpecies.h 의 kSpecies 순서와 같다 (1부터).
// 그 배열 순서를 바꾸면 여기도 같이 바꿔야 한다 — 안 그러면 모델이 통째로 밀린다.
//
// 에디터에서 DA 를 만들고 사용하는 Blueprint 기본값에 지정한다. 비어 있어도
// 동작은 한다. 못 찾은 종족은 예전처럼 큐브로 뜬다.

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UEPokemonSpeciesCatalog.generated.h"

class UUEPokemonSpeciesData;

UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEPokemonSpeciesCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// 인덱스 + 1 이 종족 id 다. 중간을 비워 두려면 그 칸을 None 으로 남기면 된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Species")
	TArray<TObjectPtr<UUEPokemonSpeciesData>> Species;

	// 범위 밖이거나 아직 안 채운 칸이면 nullptr.
	UFUNCTION(BlueprintPure, Category = "Pokemon|Species")
	UUEPokemonSpeciesData* Find(int32 SpeciesId) const;

};
