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
