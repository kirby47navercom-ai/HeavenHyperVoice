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

	/**
	 * 도감번호로 찾는다. 서버가 파트너를 지목할 때 쓰는 경로다.
	 *
	 * Find(배열 위치)를 쓰지 않는 이유는, 카탈로그에 종족을 끼워 넣으면 그 뒤가
	 * 전부 밀려 이미 저장된 파트너가 다른 종족이 되기 때문이다. 실제로 그렇게
	 * 파이리가 피카츄로 바뀌고 다른 캐릭터가 로비에서 사라진 적이 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "Pokemon|Species")
	UUEPokemonSpeciesData* FindByDex(int32 DexNumber) const;

	// FindByDex 의 역방향. 캐릭터를 만들 때 고른 데이터 에셋을 도감번호로 바꾼다.
	// 번호가 안 채워진 에셋이면 0 이다.
	UFUNCTION(BlueprintPure, Category = "Pokemon|Species")
	int32 FindDexNumber(const UUEPokemonSpeciesData* SpeciesData) const;

};
