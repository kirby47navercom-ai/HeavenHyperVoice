// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../../Map/HHVServerMapRuntime.h"
#include "UEPokemonWildPokemonSpawner.generated.h"

class AUEPokemonCharacter;
class UUEPokemonSpeciesCatalog;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Wild")
	TObjectPtr<UUEPokemonSpeciesCatalog> PokemonSpeciesCatalog = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	TSubclassOf<AUEPokemonCharacter> WildPokemonClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	bool bSpawnOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	bool bEnableLocalServerSimulation = true;

	/**
	 * 외부 FieldServer를 사용하는 중에도 이 로컬 스포너를 강제로 사용할지 정한다.
	 *
	 * 일반 플레이에서는 false로 둬야 서버 야생과 클라이언트 야생이 섞이지 않는다.
	 * FieldServer 없이 맵과 애니메이션만 확인하는 전용 테스트에서만 true로 켠다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild|Test")
	bool bAllowLocalSpawnWithExternalFieldServer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	bool bTryLoadDefaultServerMap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	FString ServerMapFilePath;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Wild")
	FString DefaultServerMapFileName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild", meta = (ClampMin = "1"))
	int32 SpawnSearchAttempts = 256;

	/** 재현 가능한 테스트가 필요할 때만 SpawnRandomSeed를 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	bool bUseFixedSpawnSeed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild")
	int32 SpawnRandomSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Wild", meta = (ClampMin = "1"))
	int32 FirstRuntimePokemonId = 100000;

private:
	bool IsExternalFieldServerConfigured() const;
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
