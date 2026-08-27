#include "UEFieldWildPokemonSyncComponent.h"

#include "../Net/HHVFieldConnection.h"
#include "../Pokemon/UEPokemonCharacter.h"

#include "Engine/World.h"

UUEFieldWildPokemonSyncComponent::UUEFieldWildPokemonSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UUEFieldWildPokemonSyncComponent::SetWildPokemonClass(TSubclassOf<AUEPokemonCharacter> InWildPokemonClass)
{
	if (InWildPokemonClass)
	{
		WildPokemonClass = InWildPokemonClass;
	}
}

bool UUEFieldWildPokemonSyncComponent::ContainsWildPokemon(uint64 EntityId) const
{
	return WildActors.Contains(EntityId);
}

void UUEFieldWildPokemonSyncComponent::HandleWildPokemonSpawned(
	const FHHVFieldEntity& Entity,
	const FVector& SpawnLocation)
{
	if (WildActors.Contains(Entity.EntityId))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSubclassOf<AUEPokemonCharacter> ClassToSpawn = WildPokemonClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AUEPokemonCharacter::StaticClass();
		UE_LOG(LogTemp, Warning,
			TEXT("FieldWildPokemonSync: WildPokemonClass is not assigned; spawning the native Pokemon class."));
	}

	const FTransform SpawnTransform(FRotator(0.0f, Entity.Facing, 0.0f), SpawnLocation);
	AUEPokemonCharacter* WildActor = World->SpawnActorDeferred<AUEPokemonCharacter>(
		ClassToSpawn,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!WildActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("FieldWildPokemonSync: wild entity %llu spawn failed"), Entity.EntityId);
		return;
	}

	WildActor->AutoPossessAI = EAutoPossessAI::Disabled;
	WildActor->AIControllerClass = nullptr;
	WildActor->FinishSpawning(SpawnTransform);
	WildActor->SetActorEnableCollision(false);
	WildActor->InitializeServerEntity(static_cast<int64>(Entity.EntityId), static_cast<int32>(Entity.Species), EUEPokemonRenderType::Wild);
	WildActors.Add(Entity.EntityId, WildActor);
}

void UUEFieldWildPokemonSyncComponent::HandleWildPokemonMoved(
	const FHHVFieldEntity& Entity,
	const FVector& TargetLocation)
{
	const TWeakObjectPtr<AUEPokemonCharacter>* Found = WildActors.Find(Entity.EntityId);
	if (!Found || !Found->IsValid())
	{
		return;
	}

	Found->Get()->ApplyServerMoveTarget(
		TargetLocation,
		FVector::ZeroVector,
		FRotator(0.0f, Entity.Facing, 0.0f),
		/*bTeleported=*/false);
}

bool UUEFieldWildPokemonSyncComponent::HandleWildPokemonDespawned(uint64 EntityId)
{
	TWeakObjectPtr<AUEPokemonCharacter> WildActor;
	if (!WildActors.RemoveAndCopyValue(EntityId, WildActor))
	{
		return false;
	}

	if (WildActor.IsValid())
	{
		WildActor->Destroy();
	}
	return true;
}

void UUEFieldWildPokemonSyncComponent::DestroyWildPokemons()
{
	for (TPair<uint64, TWeakObjectPtr<AUEPokemonCharacter>>& Pair : WildActors)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}
	WildActors.Empty();
}
