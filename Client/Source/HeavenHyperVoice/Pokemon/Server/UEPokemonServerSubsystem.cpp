#include "UEPokemonServerSubsystem.h"

#include "../UEPokemonSpeciesData.h"

void UUEPokemonServerSubsystem::RegisterOwnedPokemons(int32 OwnerServerPlayerId, const TArray<FUEPokemonServerOwnedPokemon>& OwnedPokemons)
{
	if (OwnerServerPlayerId <= 0)
	{
		return;
	}

	FUEPokemonServerPlayerState& PlayerState = PlayerStates.FindOrAdd(OwnerServerPlayerId);
	PlayerState.OwnedPokemons.Reset();

	int32 NextGeneratedInstanceId = 1;
	for (const FUEPokemonServerOwnedPokemon& OwnedPokemon : OwnedPokemons)
	{
		PlayerState.OwnedPokemons.Add(NormalizeOwnedPokemon(OwnedPokemon, NextGeneratedInstanceId));
	}

	if (PlayerState.ActiveRuntimePokemonId > 0 && !RuntimePokemons.Contains(PlayerState.ActiveRuntimePokemonId))
	{
		PlayerState.ActiveRuntimePokemonId = 0;
	}
}

void UUEPokemonServerSubsystem::ClearOwnedPokemons(int32 OwnerServerPlayerId)
{
	if (OwnerServerPlayerId <= 0)
	{
		return;
	}

	if (FUEPokemonServerPlayerState* PlayerState = PlayerStates.Find(OwnerServerPlayerId))
	{
		if (PlayerState->ActiveRuntimePokemonId > 0)
		{
			RuntimePokemons.Remove(PlayerState->ActiveRuntimePokemonId);
		}
	}

	PlayerStates.Remove(OwnerServerPlayerId);
}

FUEPokemonServerSpawnResponse UUEPokemonServerSubsystem::RequestSpawnPokemon(int32 OwnerServerPlayerId, int32 PokemonInstanceId)
{
	FUEPokemonServerSpawnResponse Response;
	Response.OwnerServerPlayerId = OwnerServerPlayerId;
	Response.PokemonInstanceId = PokemonInstanceId;

	if (OwnerServerPlayerId <= 0)
	{
		Response.Result = EUEPokemonServerSummonResult::InvalidPlayer;
		return Response;
	}

	FUEPokemonServerPlayerState* PlayerState = PlayerStates.Find(OwnerServerPlayerId);
	if (!PlayerState)
	{
		Response.Result = EUEPokemonServerSummonResult::PokemonNotOwned;
		return Response;
	}

	if (PlayerState->ActiveRuntimePokemonId > 0 && RuntimePokemons.Contains(PlayerState->ActiveRuntimePokemonId))
	{
		Response.Result = EUEPokemonServerSummonResult::PokemonAlreadySpawned;
		Response.RuntimePokemonId = PlayerState->ActiveRuntimePokemonId;
		return Response;
	}

	const FUEPokemonServerOwnedPokemon* OwnedPokemon = FindOwnedPokemon(OwnerServerPlayerId, PokemonInstanceId);
	if (!OwnedPokemon)
	{
		Response.Result = EUEPokemonServerSummonResult::PokemonNotOwned;
		return Response;
	}

	// 종족 데이터가 없으면 메시와 애니메이션을 구성할 수 없으므로 빈 포켓몬이 생성되지 않게 거절한다.
	if (!IsValid(OwnedPokemon->SpeciesData))
	{
		Response.Result = EUEPokemonServerSummonResult::MissingSpeciesData;
		return Response;
	}

	const float MaxHP = ResolveMaxHP(*OwnedPokemon);
	const float CurrentHP = FMath::Clamp(OwnedPokemon->CurrentHP, 0.0f, MaxHP);
	if (!OwnedPokemon->bCanSummon || CurrentHP <= 0.0f)
	{
		Response.Result = EUEPokemonServerSummonResult::PokemonUnavailable;
		return Response;
	}

	const int32 RuntimePokemonId = NextRuntimePokemonId++;
	FUEPokemonServerRuntimePokemon RuntimePokemon;
	RuntimePokemon.OwnerServerPlayerId = OwnerServerPlayerId;
	RuntimePokemon.RuntimePokemonId = RuntimePokemonId;
	RuntimePokemon.PokemonInstanceId = OwnedPokemon->PokemonInstanceId;
	RuntimePokemon.SpeciesId = OwnedPokemon->SpeciesId;
	RuntimePokemon.SpeciesData = OwnedPokemon->SpeciesData;
	RuntimePokemon.Level = OwnedPokemon->Level;
	RuntimePokemon.CurrentHP = CurrentHP;
	RuntimePokemon.MaxHP = MaxHP;

	RuntimePokemons.Add(RuntimePokemonId, RuntimePokemon);
	PlayerState->ActiveRuntimePokemonId = RuntimePokemonId;

	Response.bAccepted = true;
	Response.Result = EUEPokemonServerSummonResult::Success;
	Response.RuntimePokemonId = RuntimePokemonId;
	Response.PokemonInstanceId = RuntimePokemon.PokemonInstanceId;
	Response.SpeciesId = RuntimePokemon.SpeciesId;
	Response.SpeciesData = RuntimePokemon.SpeciesData;
	Response.Level = RuntimePokemon.Level;
	Response.CurrentHP = RuntimePokemon.CurrentHP;
	Response.MaxHP = RuntimePokemon.MaxHP;
	return Response;
}

bool UUEPokemonServerSubsystem::RequestDespawnPokemon(int32 OwnerServerPlayerId, int32 RuntimePokemonId)
{
	if (OwnerServerPlayerId <= 0 || RuntimePokemonId <= 0)
	{
		return false;
	}

	FUEPokemonServerRuntimePokemon RuntimePokemon;
	if (!RuntimePokemons.RemoveAndCopyValue(RuntimePokemonId, RuntimePokemon))
	{
		return false;
	}

	if (RuntimePokemon.OwnerServerPlayerId != OwnerServerPlayerId)
	{
		RuntimePokemons.Add(RuntimePokemonId, RuntimePokemon);
		return false;
	}

	if (FUEPokemonServerOwnedPokemon* OwnedPokemon = FindMutableOwnedPokemon(OwnerServerPlayerId, RuntimePokemon.PokemonInstanceId))
	{
		OwnedPokemon->CurrentHP = FMath::Clamp(RuntimePokemon.CurrentHP, 0.0f, RuntimePokemon.MaxHP);
	}

	if (FUEPokemonServerPlayerState* PlayerState = PlayerStates.Find(OwnerServerPlayerId))
	{
		if (PlayerState->ActiveRuntimePokemonId == RuntimePokemonId)
		{
			PlayerState->ActiveRuntimePokemonId = 0;
		}
	}

	return true;
}

bool UUEPokemonServerSubsystem::HasActivePokemon(int32 OwnerServerPlayerId) const
{
	const FUEPokemonServerPlayerState* PlayerState = PlayerStates.Find(OwnerServerPlayerId);
	return PlayerState && PlayerState->ActiveRuntimePokemonId > 0 && RuntimePokemons.Contains(PlayerState->ActiveRuntimePokemonId);
}

bool UUEPokemonServerSubsystem::TryGetOwnedPokemon(int32 OwnerServerPlayerId, int32 PokemonInstanceId, FUEPokemonServerOwnedPokemon& OutOwnedPokemon) const
{
	const FUEPokemonServerOwnedPokemon* OwnedPokemon = FindOwnedPokemon(OwnerServerPlayerId, PokemonInstanceId);
	if (!OwnedPokemon)
	{
		return false;
	}

	OutOwnedPokemon = *OwnedPokemon;
	return true;
}

FUEPokemonServerOwnedPokemon UUEPokemonServerSubsystem::NormalizeOwnedPokemon(const FUEPokemonServerOwnedPokemon& OwnedPokemon, int32& NextGeneratedInstanceId) const
{
	FUEPokemonServerOwnedPokemon NormalizedPokemon = OwnedPokemon;
	if (NormalizedPokemon.PokemonInstanceId <= 0)
	{
		NormalizedPokemon.PokemonInstanceId = NextGeneratedInstanceId;
	}

	NextGeneratedInstanceId = FMath::Max(NextGeneratedInstanceId, NormalizedPokemon.PokemonInstanceId + 1);

	if (NormalizedPokemon.SpeciesId.IsNone() && NormalizedPokemon.SpeciesData)
	{
		NormalizedPokemon.SpeciesId = NormalizedPokemon.SpeciesData->SpeciesId;
	}
	// 종족 데이터가 비어 있으면 서버 기본값도 비워 둔다.

	NormalizedPokemon.Level = FMath::Max(NormalizedPokemon.Level, 1);

	const float MaxHP = ResolveMaxHP(NormalizedPokemon);
	if (NormalizedPokemon.CurrentHP <= 0.0f)
	{
		NormalizedPokemon.CurrentHP = MaxHP;
	}
	else
	{
		NormalizedPokemon.CurrentHP = FMath::Clamp(NormalizedPokemon.CurrentHP, 0.0f, MaxHP);
	}

	return NormalizedPokemon;
}

const FUEPokemonServerOwnedPokemon* UUEPokemonServerSubsystem::FindOwnedPokemon(int32 OwnerServerPlayerId, int32 PokemonInstanceId) const
{
	const FUEPokemonServerPlayerState* PlayerState = PlayerStates.Find(OwnerServerPlayerId);
	if (!PlayerState)
	{
		return nullptr;
	}

	if (PokemonInstanceId <= 0 && !PlayerState->OwnedPokemons.IsEmpty())
	{
		return &PlayerState->OwnedPokemons[0];
	}

	return PlayerState->OwnedPokemons.FindByPredicate(
		[PokemonInstanceId](const FUEPokemonServerOwnedPokemon& OwnedPokemon)
		{
			return OwnedPokemon.PokemonInstanceId == PokemonInstanceId;
		});
}

FUEPokemonServerOwnedPokemon* UUEPokemonServerSubsystem::FindMutableOwnedPokemon(int32 OwnerServerPlayerId, int32 PokemonInstanceId)
{
	FUEPokemonServerPlayerState* PlayerState = PlayerStates.Find(OwnerServerPlayerId);
	if (!PlayerState)
	{
		return nullptr;
	}

	return PlayerState->OwnedPokemons.FindByPredicate(
		[PokemonInstanceId](const FUEPokemonServerOwnedPokemon& OwnedPokemon)
		{
			return OwnedPokemon.PokemonInstanceId == PokemonInstanceId;
		});
}

float UUEPokemonServerSubsystem::ResolveMaxHP(const FUEPokemonServerOwnedPokemon& OwnedPokemon) const
{
	if (OwnedPokemon.SpeciesData)
	{
		return FMath::Max(OwnedPokemon.SpeciesData->MaxHP, 1.0f);
	}

	return OwnedPokemon.CurrentHP > 0.0f ? FMath::Max(OwnedPokemon.CurrentHP, 1.0f) : 100.0f;
}
