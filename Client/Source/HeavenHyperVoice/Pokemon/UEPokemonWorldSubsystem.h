#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UEPokemonCharacter.h"
#include "UEPokemonWorldSubsystem.generated.h"

class UUEPokemonSpeciesData;

USTRUCT(BlueprintType)
struct FUEPokemonWorldSpawnData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World")
	int32 RuntimePokemonId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World")
	int32 OwnerServerPlayerId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World")
	int32 PokemonInstanceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World")
	FName SpeciesId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World")
	EUEPokemonRenderType RenderType = EUEPokemonRenderType::Wild;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World")
	TObjectPtr<UUEPokemonSpeciesData> SpeciesData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World")
	TSubclassOf<AUEPokemonCharacter> PokemonClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World", meta = (ClampMin = "0.0"))
	float CurrentHP = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World", meta = (ClampMin = "0.0"))
	float MaxHP = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World")
	bool bEnableLocalServerSimulation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|World")
	EUEPokemonServerSimulationMode ServerSimulationMode = EUEPokemonServerSimulationMode::FollowOwner;
};

USTRUCT(BlueprintType)
struct FUEPokemonWorldEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|World")
	int32 RuntimePokemonId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|World")
	int32 OwnerServerPlayerId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|World")
	int32 PokemonInstanceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|World")
	FName SpeciesId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|World")
	EUEPokemonRenderType RenderType = EUEPokemonRenderType::Wild;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|World")
	TObjectPtr<AUEPokemonCharacter> PokemonActor = nullptr;
};

UCLASS()
class HEAVENHYPERVOICE_API UUEPokemonWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Pokemon|World")
	AUEPokemonCharacter* SpawnOrUpdatePokemon(const FUEPokemonWorldSpawnData& SpawnData);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|World")
	bool RegisterExistingPokemon(AUEPokemonCharacter* PokemonActor, EUEPokemonRenderType RenderType, int32 OwnerServerPlayerId = 0);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|World")
	bool ApplyPokemonSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|World")
	bool DespawnPokemon(int32 RuntimePokemonId, bool bDestroyActor = true);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|World")
	void ClearRenderedPokemons(bool bDestroyActors = true);

	UFUNCTION(BlueprintPure, Category = "Pokemon|World")
	AUEPokemonCharacter* FindRenderedPokemon(int32 RuntimePokemonId) const;

	UFUNCTION(BlueprintPure, Category = "Pokemon|World")
	bool IsPokemonRendered(int32 RuntimePokemonId) const;

	UFUNCTION(BlueprintPure, Category = "Pokemon|World")
	TArray<AUEPokemonCharacter*> GetRenderedPokemons() const;

	UFUNCTION(BlueprintPure, Category = "Pokemon|World")
	TArray<AUEPokemonCharacter*> GetRenderedPokemonsByType(EUEPokemonRenderType RenderType) const;

private:
	FUEPokemonServerMoveSnapshot MakeInitialSnapshot(const FUEPokemonWorldSpawnData& SpawnData) const;
	void UpdateEntryFromSpawnData(FUEPokemonWorldEntry& Entry, AUEPokemonCharacter* PokemonActor, const FUEPokemonWorldSpawnData& SpawnData);
	void PruneInvalidEntries();

	UPROPERTY(Transient)
	TMap<int32, FUEPokemonWorldEntry> RenderedPokemons;
};
