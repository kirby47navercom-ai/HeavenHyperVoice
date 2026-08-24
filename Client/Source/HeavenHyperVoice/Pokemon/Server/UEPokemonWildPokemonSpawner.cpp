// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPokemonWildPokemonSpawner.h"

#include "../UEPokemonCharacter.h"
#include "../UEPokemonSpeciesCatalog.h"
#include "../UEPokemonSpeciesData.h"
#include "../UEPokemonWorldSubsystem.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void ShuffleSpeciesPool(TArray<UUEPokemonSpeciesData*>& SpeciesPool, FRandomStream& RandomStream)
	{
		for (int32 Index = SpeciesPool.Num() - 1; Index > 0; --Index)
		{
			SpeciesPool.Swap(Index, RandomStream.RandRange(0, Index));
		}
	}

	int32 MakeRuntimeSpawnSeed()
	{
		const FGuid SessionGuid = FGuid::NewGuid();
		const uint64 Cycles = FPlatformTime::Cycles64();
		const uint32 MixedSeed = SessionGuid.A ^ SessionGuid.B ^ SessionGuid.C ^ SessionGuid.D
			^ static_cast<uint32>(Cycles) ^ static_cast<uint32>(Cycles >> 32);
		return MixedSeed == 0 ? 1 : static_cast<int32>(MixedSeed);
	}
}

AUEPokemonWildPokemonSpawner::AUEPokemonWildPokemonSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FClassFinder<AUEPokemonCharacter> PokemonClassFinder(TEXT("/Game/Pokemon/BP_Pokemon"));
	if (PokemonClassFinder.Succeeded())
	{
		WildPokemonClass = PokemonClassFinder.Class;
	}
}

void AUEPokemonWildPokemonSpawner::BeginPlay()
{
	Super::BeginPlay();

	NextRuntimePokemonId = FMath::Max(FirstRuntimePokemonId, 1);
	if (bSpawnOnBeginPlay)
	{
		SpawnWildPokemons();
	}
}

int32 AUEPokemonWildPokemonSpawner::SpawnWildPokemons()
{
	// 월드의 야생 포켓몬은 서버에서만 생성해야 모든 클라이언트가 같은 결과를 봅니다.
	if (!HasAuthority())
	{
		return 0;
	}

	if (bHasSpawnedWildPokemons)
	{
		return 0;
	}

	if (WildPokemonCount <= 0 || !TryLoadServerMap())
	{
		return 0;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	UUEPokemonWorldSubsystem* PokemonWorldSubsystem = World->GetSubsystem<UUEPokemonWorldSubsystem>();
	if (!PokemonWorldSubsystem)
	{
		return 0;
	}

	TSubclassOf<AUEPokemonCharacter> ClassToSpawn = WildPokemonClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AUEPokemonCharacter::StaticClass();
	}

	// 명시적으로 지정한 후보가 없으면 종족 카탈로그 전체를 사용합니다.
	TArray<UUEPokemonSpeciesData*> ValidSpeciesPool;
	ValidSpeciesPool.Reserve(WildPokemonSpeciesPool.Num());
	for (UUEPokemonSpeciesData* SpeciesData : WildPokemonSpeciesPool)
	{
		if (IsValid(SpeciesData))
		{
			ValidSpeciesPool.Add(SpeciesData);
		}
	}
	if (ValidSpeciesPool.IsEmpty())
	{
		if (const UUEPokemonSpeciesCatalog* SpeciesCatalog = UUEPokemonSpeciesCatalog::Get())
		{
			for (UUEPokemonSpeciesData* SpeciesData : SpeciesCatalog->Species)
			{
				if (IsValid(SpeciesData))
				{
					ValidSpeciesPool.AddUnique(SpeciesData);
				}
			}
		}
	}
	if (ValidSpeciesPool.IsEmpty() && IsValid(WildPokemonSpeciesData))
	{
		ValidSpeciesPool.Add(WildPokemonSpeciesData);
	}
	ShuffleSpeciesPool(ValidSpeciesPool, SpawnRandomStream);

	int32 SpawnedCount = 0;
	for (int32 SpawnIndex = 0; SpawnIndex < WildPokemonCount; ++SpawnIndex)
	{
		if (SpawnIndex > 0 && !ValidSpeciesPool.IsEmpty() && SpawnIndex % ValidSpeciesPool.Num() == 0)
		{
			ShuffleSpeciesPool(ValidSpeciesPool, SpawnRandomStream);
		}
		UUEPokemonSpeciesData* SelectedSpeciesData = ValidSpeciesPool.IsEmpty()
			? nullptr
			: ValidSpeciesPool[SpawnIndex % ValidSpeciesPool.Num()];
		const HHV::Map::AgentSettings Agent = MakeWildSpawnAgentSettings(SelectedSpeciesData);

		FVector SpawnLocation;
		if (!TryFindRandomWildSpawnLocation(Agent, SpawnLocation))
		{
			continue;
		}

		const float MaxHP = SelectedSpeciesData ? FMath::Max(SelectedSpeciesData->MaxHP, 1.0f) : 0.0f;

		FUEPokemonWorldSpawnData SpawnData;
		SpawnData.RuntimePokemonId = NextRuntimePokemonId++;
		// 종족 데이터가 없을 때 임의 포켓몬 이름으로 고정하지 않는다.
		SpawnData.SpeciesId = SelectedSpeciesData ? SelectedSpeciesData->SpeciesId : NAME_None;
		SpawnData.RenderType = EUEPokemonRenderType::Wild;
		SpawnData.SpeciesData = SelectedSpeciesData;
		SpawnData.PokemonClass = ClassToSpawn;
		SpawnData.Location = SpawnLocation;
		SpawnData.Rotation = FRotator(0.0f, SpawnRandomStream.FRandRange(0.0f, 360.0f), 0.0f);
		SpawnData.CurrentHP = MaxHP;
		SpawnData.MaxHP = MaxHP;
		SpawnData.bEnableLocalServerSimulation = bEnableLocalServerSimulation;
		SpawnData.ServerSimulationMode = EUEPokemonServerSimulationMode::Wander;

		if (PokemonWorldSubsystem->SpawnOrUpdatePokemon(SpawnData))
		{
			++SpawnedCount;
		}
	}

	bHasSpawnedWildPokemons = SpawnedCount > 0;
	return SpawnedCount;
}

bool AUEPokemonWildPokemonSpawner::TryLoadServerMap()
{
	if (bServerMapLoaded)
	{
		return true;
	}

	const FString MapFilePath = ResolveServerMapFilePath();
	if (MapFilePath.IsEmpty() || !FPaths::FileExists(MapFilePath))
	{
		return false;
	}

	bServerMapLoaded = ServerMapRuntime.LoadFromFile(TCHAR_TO_UTF8(*MapFilePath));
	if (bServerMapLoaded && !bRandomStreamInitialized)
	{
		const int32 Seed = bUseFixedSpawnSeed ? SpawnRandomSeed : MakeRuntimeSpawnSeed();
		SpawnRandomStream.Initialize(Seed);
		bRandomStreamInitialized = true;
		UE_LOG(LogTemp, Log, TEXT("[POKEMON] wild spawn random seed: %d%s"),
			Seed, bUseFixedSpawnSeed ? TEXT(" (fixed)") : TEXT(""));
	}
	return bServerMapLoaded;
}

bool AUEPokemonWildPokemonSpawner::TryFindRandomWildSpawnLocation(const HHV::Map::AgentSettings& Agent, FVector& OutLocation)
{
	if (!TryLoadServerMap())
	{
		return false;
	}

	const HHV::Map::HeightMap& HeightMap = ServerMapRuntime.GetHeightMap();
	if (HeightMap.Width <= 0 || HeightMap.Height <= 0 || HeightMap.CellSize <= 0.0f || HeightMap.Cells.empty())
	{
		return false;
	}

	for (int32 AttemptIndex = 0; AttemptIndex < FMath::Max(SpawnSearchAttempts, 1); ++AttemptIndex)
	{
		const int32 CellX = SpawnRandomStream.RandRange(0, HeightMap.Width - 1);
		const int32 CellY = SpawnRandomStream.RandRange(0, HeightMap.Height - 1);
		const int32 CellIndex = CellY * HeightMap.Width + CellX;
		if (CellIndex < 0 || CellIndex >= static_cast<int32>(HeightMap.Cells.size()) || !HeightMap.Cells[static_cast<std::size_t>(CellIndex)].bHasGround)
		{
			continue;
		}

		const HHV::Map::Vec3 Candidate{
			HeightMap.Origin.X + (static_cast<float>(CellX) + SpawnRandomStream.FRand()) * HeightMap.CellSize,
			HeightMap.Origin.Y + (static_cast<float>(CellY) + SpawnRandomStream.FRand()) * HeightMap.CellSize,
			HeightMap.Cells[static_cast<std::size_t>(CellIndex)].Height + Agent.CapsuleHalfHeight
		};

		HHV::Map::Vec3 GroundedLocation;
		if (ServerMapRuntime.IsWalkableLocation(Candidate, Agent, &GroundedLocation))
		{
			OutLocation = FVector(GroundedLocation.X, GroundedLocation.Y, GroundedLocation.Z);
			return true;
		}
	}

	return false;
}

HHV::Map::AgentSettings AUEPokemonWildPokemonSpawner::MakeWildSpawnAgentSettings(
	const UUEPokemonSpeciesData* SpeciesData) const
{
	HHV::Map::AgentSettings Agent;
	if (SpeciesData)
	{
		Agent.CapsuleRadius = SpeciesData->CapsuleRadius;
		Agent.CapsuleHalfHeight = SpeciesData->CapsuleHalfHeight;
		Agent.MaxStepHeight = SpeciesData->MaxStepHeight;
		Agent.WalkableFloorAngleDegrees = SpeciesData->WalkableFloorAngleDegrees;
		return Agent;
	}

	TSubclassOf<AUEPokemonCharacter> ClassToInspect = WildPokemonClass;
	if (!ClassToInspect)
	{
		ClassToInspect = AUEPokemonCharacter::StaticClass();
	}

	const AUEPokemonCharacter* DefaultPokemon = ClassToInspect ? ClassToInspect->GetDefaultObject<AUEPokemonCharacter>() : nullptr;
	if (!DefaultPokemon)
	{
		return Agent;
	}

	if (const UCapsuleComponent* CapsuleComponent = DefaultPokemon->GetCapsuleComponent())
	{
		Agent.CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
		Agent.CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	}

	if (const UCharacterMovementComponent* MovementComponent = DefaultPokemon->GetCharacterMovement())
	{
		Agent.MaxStepHeight = MovementComponent->MaxStepHeight;
		Agent.WalkableFloorAngleDegrees = MovementComponent->GetWalkableFloorAngle();
	}

	return Agent;
}

FString AUEPokemonWildPokemonSpawner::ResolveServerMapFilePath() const
{
	if (!ServerMapFilePath.IsEmpty())
	{
		return ServerMapFilePath;
	}

	if (!bTryLoadDefaultServerMap)
	{
		return FString();
	}

	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("ServerMaps"),
		TEXT("PlayerTestLevel.hhvservermap")
	);
}
