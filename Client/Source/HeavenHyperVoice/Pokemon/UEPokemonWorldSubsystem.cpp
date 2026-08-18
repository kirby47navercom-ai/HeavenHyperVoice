#include "UEPokemonWorldSubsystem.h"

#include "Server/UEPokemonServerComponent.h"
#include "UEPokemonSpeciesData.h"

#include "Engine/World.h"

void UUEPokemonWorldSubsystem::Deinitialize()
{
	ClearRenderedPokemons(false);
	Super::Deinitialize();
}

AUEPokemonCharacter* UUEPokemonWorldSubsystem::SpawnOrUpdatePokemon(const FUEPokemonWorldSpawnData& SpawnData)
{
	if (SpawnData.RuntimePokemonId <= 0)
	{
		return nullptr;
	}

	PruneInvalidEntries();

	if (FUEPokemonWorldEntry* ExistingEntry = RenderedPokemons.Find(SpawnData.RuntimePokemonId))
	{
		AUEPokemonCharacter* ExistingPokemon = ExistingEntry->PokemonActor;
		if (!IsValid(ExistingPokemon))
		{
			RenderedPokemons.Remove(SpawnData.RuntimePokemonId);
		}
		else
		{
			UpdateEntryFromSpawnData(*ExistingEntry, ExistingPokemon, SpawnData);
			ExistingPokemon->ApplyServerMoveSnapshot(MakeInitialSnapshot(SpawnData));
			return ExistingPokemon;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TSubclassOf<AUEPokemonCharacter> ClassToSpawn = SpawnData.PokemonClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AUEPokemonCharacter::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AUEPokemonCharacter* NewPokemon = World->SpawnActor<AUEPokemonCharacter>(ClassToSpawn, SpawnData.Location, SpawnData.Rotation, SpawnParameters);
	if (!NewPokemon)
	{
		return nullptr;
	}

	FUEPokemonWorldEntry& NewEntry = RenderedPokemons.FindOrAdd(SpawnData.RuntimePokemonId);
	UpdateEntryFromSpawnData(NewEntry, NewPokemon, SpawnData);

	if (UUEPokemonServerComponent* ServerComponent = NewPokemon->GetServerComponent())
	{
		ServerComponent->SetServerSimulationMode(SpawnData.ServerSimulationMode);
		ServerComponent->SetServerSimulationEnabled(SpawnData.bEnableLocalServerSimulation);
		ServerComponent->InitializeServerRuntimePokemon(
			SpawnData.RuntimePokemonId,
			SpawnData.PokemonInstanceId,
			SpawnData.CurrentHP,
			SpawnData.MaxHP
		);
	}

	NewPokemon->ApplyServerMoveSnapshot(MakeInitialSnapshot(SpawnData));
	return NewPokemon;
}

bool UUEPokemonWorldSubsystem::RegisterExistingPokemon(AUEPokemonCharacter* PokemonActor, EUEPokemonRenderType RenderType, int32 OwnerServerPlayerId)
{
	if (!IsValid(PokemonActor) || PokemonActor->GetServerPokemonId() <= 0)
	{
		return false;
	}

	FUEPokemonWorldEntry& Entry = RenderedPokemons.FindOrAdd(PokemonActor->GetServerPokemonId());
	Entry.RuntimePokemonId = PokemonActor->GetServerPokemonId();
	Entry.OwnerServerPlayerId = OwnerServerPlayerId;
	Entry.PokemonInstanceId = PokemonActor->GetPokemonInstanceId();
	Entry.SpeciesId = PokemonActor->GetPokemonSpeciesId();
	Entry.RenderType = RenderType;
	Entry.PokemonActor = PokemonActor;
	PokemonActor->SetRenderType(RenderType);
	return true;
}

bool UUEPokemonWorldSubsystem::ApplyPokemonSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot)
{
	if (Snapshot.PokemonId <= 0)
	{
		return false;
	}

	AUEPokemonCharacter* PokemonActor = FindRenderedPokemon(Snapshot.PokemonId);
	if (!PokemonActor)
	{
		return false;
	}

	PokemonActor->ApplyServerMoveSnapshot(Snapshot);
	if (FUEPokemonWorldEntry* Entry = RenderedPokemons.Find(Snapshot.PokemonId))
	{
		Entry->PokemonInstanceId = Snapshot.PokemonInstanceId;
		Entry->SpeciesId = Snapshot.SpeciesId;
		Entry->RenderType = Snapshot.RenderType;
	}
	return true;
}

bool UUEPokemonWorldSubsystem::DespawnPokemon(int32 RuntimePokemonId, bool bDestroyActor)
{
	FUEPokemonWorldEntry Entry;
	if (!RenderedPokemons.RemoveAndCopyValue(RuntimePokemonId, Entry))
	{
		return false;
	}

	if (bDestroyActor && IsValid(Entry.PokemonActor))
	{
		Entry.PokemonActor->Destroy();
	}

	return true;
}

void UUEPokemonWorldSubsystem::ClearRenderedPokemons(bool bDestroyActors)
{
	if (bDestroyActors)
	{
		for (const TPair<int32, FUEPokemonWorldEntry>& EntryPair : RenderedPokemons)
		{
			if (IsValid(EntryPair.Value.PokemonActor))
			{
				EntryPair.Value.PokemonActor->Destroy();
			}
		}
	}

	RenderedPokemons.Reset();
}

AUEPokemonCharacter* UUEPokemonWorldSubsystem::FindRenderedPokemon(int32 RuntimePokemonId) const
{
	const FUEPokemonWorldEntry* Entry = RenderedPokemons.Find(RuntimePokemonId);
	return Entry && IsValid(Entry->PokemonActor) ? Entry->PokemonActor : nullptr;
}

bool UUEPokemonWorldSubsystem::IsPokemonRendered(int32 RuntimePokemonId) const
{
	return FindRenderedPokemon(RuntimePokemonId) != nullptr;
}

TArray<AUEPokemonCharacter*> UUEPokemonWorldSubsystem::GetRenderedPokemons() const
{
	TArray<AUEPokemonCharacter*> Result;
	Result.Reserve(RenderedPokemons.Num());
	for (const TPair<int32, FUEPokemonWorldEntry>& EntryPair : RenderedPokemons)
	{
		if (IsValid(EntryPair.Value.PokemonActor))
		{
			Result.Add(EntryPair.Value.PokemonActor);
		}
	}
	return Result;
}

TArray<AUEPokemonCharacter*> UUEPokemonWorldSubsystem::GetRenderedPokemonsByType(EUEPokemonRenderType RenderType) const
{
	TArray<AUEPokemonCharacter*> Result;
	for (const TPair<int32, FUEPokemonWorldEntry>& EntryPair : RenderedPokemons)
	{
		if (EntryPair.Value.RenderType == RenderType && IsValid(EntryPair.Value.PokemonActor))
		{
			Result.Add(EntryPair.Value.PokemonActor);
		}
	}
	return Result;
}

FUEPokemonServerMoveSnapshot UUEPokemonWorldSubsystem::MakeInitialSnapshot(const FUEPokemonWorldSpawnData& SpawnData) const
{
	const FName ResolvedSpeciesId = !SpawnData.SpeciesId.IsNone()
		? SpawnData.SpeciesId
		: (SpawnData.SpeciesData ? SpawnData.SpeciesData->SpeciesId : NAME_None);

	FUEPokemonServerMoveSnapshot Snapshot;
	Snapshot.PokemonId = SpawnData.RuntimePokemonId;
	Snapshot.PokemonInstanceId = SpawnData.PokemonInstanceId;
	Snapshot.SpeciesId = ResolvedSpeciesId;
	Snapshot.RenderType = SpawnData.RenderType;
	Snapshot.CurrentHP = SpawnData.CurrentHP;
	Snapshot.MaxHP = SpawnData.MaxHP;
	Snapshot.Location = SpawnData.Location;
	Snapshot.Rotation = SpawnData.Rotation;
	Snapshot.bTeleported = true;
	Snapshot.AnimationState = EUEPokemonAnimationState::Idle;
	return Snapshot;
}

void UUEPokemonWorldSubsystem::UpdateEntryFromSpawnData(FUEPokemonWorldEntry& Entry, AUEPokemonCharacter* PokemonActor, const FUEPokemonWorldSpawnData& SpawnData)
{
	const FName ResolvedSpeciesId = !SpawnData.SpeciesId.IsNone()
		? SpawnData.SpeciesId
		: (SpawnData.SpeciesData ? SpawnData.SpeciesData->SpeciesId : NAME_None);

	Entry.RuntimePokemonId = SpawnData.RuntimePokemonId;
	Entry.OwnerServerPlayerId = SpawnData.OwnerServerPlayerId;
	Entry.PokemonInstanceId = SpawnData.PokemonInstanceId;
	Entry.SpeciesId = ResolvedSpeciesId;
	Entry.RenderType = SpawnData.RenderType;
	Entry.PokemonActor = PokemonActor;

	PokemonActor->SetRenderType(SpawnData.RenderType);
	if (SpawnData.SpeciesData)
	{
		PokemonActor->SetPokemonSpeciesData(SpawnData.SpeciesData);
	}
	if (SpawnData.MaxHP > 0.0f)
	{
		PokemonActor->ApplyServerStats(SpawnData.CurrentHP, SpawnData.MaxHP);
	}
}

void UUEPokemonWorldSubsystem::PruneInvalidEntries()
{
	for (auto Iterator = RenderedPokemons.CreateIterator(); Iterator; ++Iterator)
	{
		if (!IsValid(Iterator.Value().PokemonActor))
		{
			Iterator.RemoveCurrent();
		}
	}
}
