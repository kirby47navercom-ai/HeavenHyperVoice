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

int32 UUEPokemonSpeciesCatalog::FindSpeciesId(const UUEPokemonSpeciesData* SpeciesData) const
{
	if (SpeciesData == nullptr)
	{
		return 0;
	}
	for (int32 Index = 0; Index < Species.Num(); ++Index)
	{
		if (Species[Index] == SpeciesData)
		{
			return Index + 1;
		}
	}
	return 0;
}
