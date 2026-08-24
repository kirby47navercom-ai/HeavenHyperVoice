#include "UEPokemonSpeciesCatalog.h"

#include "UEPokemonSpeciesData.h"

namespace
{
	// 한 번 찾고 결과를 기억한다. 없는 경우도 기억해서 매 스폰마다 디스크를
	// 두드리지 않는다 (야생이 시야에 들락날락할 때마다 불린다).
	bool bCatalogResolved = false;
	TWeakObjectPtr<const UUEPokemonSpeciesCatalog> CachedCatalog;
}

const TCHAR* UUEPokemonSpeciesCatalog::CatalogAssetPath()
{
	return TEXT("/Game/Pokemon/DA_PokemonSpeciesCatalog.DA_PokemonSpeciesCatalog");
}

UUEPokemonSpeciesData* UUEPokemonSpeciesCatalog::Find(int32 SpeciesId) const
{
	const int32 Index = SpeciesId - 1;
	if (!Species.IsValidIndex(Index))
	{
		return nullptr;
	}
	return Species[Index];
}

const UUEPokemonSpeciesCatalog* UUEPokemonSpeciesCatalog::Get()
{
	if (bCatalogResolved)
	{
		return CachedCatalog.Get();
	}
	bCatalogResolved = true;

	const UUEPokemonSpeciesCatalog* Catalog =
		LoadObject<UUEPokemonSpeciesCatalog>(nullptr, CatalogAssetPath());
	CachedCatalog = Catalog;

	// 아직 에셋이 없는 상태가 정상이다. 경고 한 번만 남기고 큐브로 넘어간다.
	UE_LOG(LogTemp, Warning, TEXT("[POKEMON] species catalog %s: %s"),
		Catalog ? TEXT("loaded") : TEXT("not found"), CatalogAssetPath());
	if (Catalog)
	{
		UE_LOG(LogTemp, Warning, TEXT("[POKEMON] catalog entries: %d"), Catalog->Species.Num());
	}
	return Catalog;
}
