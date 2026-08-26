#include "UEPokemonSpeciesCatalog.h"

#include "UEPokemonSpeciesData.h"

UUEPokemonSpeciesData* UUEPokemonSpeciesCatalog::Find(int32 SpeciesId) const
{
	const int32 Index = SpeciesId - 1;
	if (!Species.IsValidIndex(Index))
	{
		return nullptr;
	}
	return Species[Index];
}

UUEPokemonSpeciesData* UUEPokemonSpeciesCatalog::FindByDex(int32 DexNumber) const
{
	if (DexNumber <= 0)
	{
		return nullptr;
	}

	// 도감번호는 연속이 아니라(꼬부기 7, 피카츄 25, 벼리짱 1105) 인덱스로 못 짚는다.
	// 20 종이라 선형 탐색으로 충분하다.
	for (UUEPokemonSpeciesData* Entry : Species)
	{
		if (Entry && Entry->DexNumber == DexNumber)
		{
			return Entry;
		}
	}
	return nullptr;
}

int32 UUEPokemonSpeciesCatalog::FindDexNumber(const UUEPokemonSpeciesData* SpeciesData) const
{
	return SpeciesData ? SpeciesData->DexNumber : 0;
}
