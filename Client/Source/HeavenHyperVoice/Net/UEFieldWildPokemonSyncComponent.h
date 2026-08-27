#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UEFieldWildPokemonSyncComponent.generated.h"

class AUEPokemonCharacter;
struct FHHVFieldEntity;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEFieldWildPokemonSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEFieldWildPokemonSyncComponent();

	void SetWildPokemonClass(TSubclassOf<AUEPokemonCharacter> InWildPokemonClass);
	bool ContainsWildPokemon(uint64 EntityId) const;
	void HandleWildPokemonSpawned(const FHHVFieldEntity& Entity, const FVector& SpawnLocation);
	void HandleWildPokemonMoved(const FHHVFieldEntity& Entity, const FVector& TargetLocation);
	bool HandleWildPokemonDespawned(uint64 EntityId);
	void DestroyWildPokemons();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Wild Pokemon")
	TSubclassOf<AUEPokemonCharacter> WildPokemonClass;

private:
	TMap<uint64, TWeakObjectPtr<AUEPokemonCharacter>> WildActors;
};
