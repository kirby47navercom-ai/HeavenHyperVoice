#pragma once

// 종족 id -> 그 종족의 비주얼 데이터.
//
// 서버는 종족을 번호로만 보낸다 (field.fbs 의 EntityState.species). 그 번호로
// 메시·캡슐·애니메이션을 찾을 표가 지금까지 없어서, 야생 포켓몬은 전부 색만
// 다른 큐브로 떴다. 이게 그 표다.
//
// 조회는 도감번호로만 한다. 배열 순서에는 아무 뜻이 없으니 편한 대로 넣어도 된다 —
// 예전에는 인덱스가 서버 내부 id 였고, 팀원이 중간에 한 줄 끼우자 그 뒤가 전부
// 밀려서 이미 저장된 파트너가 다른 종족이 됐다.
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
	// 순서는 상관없다. 빈 칸이 있어도 되고 (FindByDex 가 걸러낸다).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Species")
	TArray<TObjectPtr<UUEPokemonSpeciesData>> Species;

	/**
	 * 도감번호로 찾는다. 서버가 파트너를 지목할 때 쓰는 경로다.
	 *
	 * 배열 위치로 찾는 함수는 두지 않는다. 카탈로그에 종족을 끼워 넣으면 그 뒤가
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
