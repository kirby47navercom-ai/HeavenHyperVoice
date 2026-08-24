// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../../Map/HHVServerMapRuntime.h"
#include "UEPokemonWildPokemonSpawner.generated.h"

class AUEPokemonCharacter;
class UUEPokemonSpeciesData;

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEPokemonWildPokemonSpawner : public AActor
{
	GENERATED_BODY()

public:
	AUEPokemonWildPokemonSpawner();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	int32 SpawnWildPokemons();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild", meta = (ClampMin = "0"))
	int32 WildPokemonCount = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	TObjectPtr<UUEPokemonSpeciesData> WildPokemonSpeciesData = nullptr;

	/** 야생 포켓몬을 한 마리 소환할 때마다 이 목록에서 종을 무작위로 선택합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	TArray<TObjectPtr<UUEPokemonSpeciesData>> WildPokemonSpeciesPool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	TSubclassOf<AUEPokemonCharacter> WildPokemonClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	bool bSpawnOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	bool bEnableLocalServerSimulation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	bool bTryLoadDefaultServerMap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	FString ServerMapFilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild", meta = (ClampMin = "1"))
	int32 SpawnSearchAttempts = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	int32 SpawnRandomSeed = 20260818;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild", meta = (ClampMin = "1"))
	int32 FirstRuntimePokemonId = 100000;

private:
	bool TryLoadServerMap();
	bool TryFindRandomWildSpawnLocation(const HHV::Map::AgentSettings& Agent, FVector& OutLocation);
	HHV::Map::AgentSettings MakeWildSpawnAgentSettings(const UUEPokemonSpeciesData* SpeciesData) const;
	FString ResolveServerMapFilePath() const;

	HHV::Map::ServerMapRuntime ServerMapRuntime;
	FRandomStream SpawnRandomStream;
	bool bServerMapLoaded = false;
	bool bRandomStreamInitialized = false;
	bool bHasSpawnedWildPokemons = false;
	int32 NextRuntimePokemonId = 100000;
};
