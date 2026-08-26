#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UEPokemonServerSubsystem.generated.h"

class UUEPokemonSpeciesData;

// 보유 포켓몬 목록이 바뀌었을 때 HUD가 다시 그릴 수 있도록 알리는 델리게이트다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FUEPokemonOwnedRosterChangedSignature,
	int32, OwnerServerPlayerId);

UENUM(BlueprintType)
enum class EUEPokemonServerSummonResult : uint8
{
	Success,
	InvalidPlayer,
	PokemonNotOwned,
	PokemonUnavailable,
	PokemonAlreadySpawned,
	MissingSpeciesData
};

USTRUCT(BlueprintType)
struct FUEPokemonServerOwnedPokemon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server")
	int32 PokemonInstanceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server")
	FName SpeciesId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server")
	TObjectPtr<UUEPokemonSpeciesData> SpeciesData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (ClampMin = "1"))
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (ClampMin = "0.0"))
	float CurrentHP = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server")
	bool bCanSummon = true;
};

USTRUCT(BlueprintType)
struct FUEPokemonServerSpawnResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|Server")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|Server")
	EUEPokemonServerSummonResult Result = EUEPokemonServerSummonResult::InvalidPlayer;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|Server")
	int32 OwnerServerPlayerId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|Server")
	int32 RuntimePokemonId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|Server")
	int32 PokemonInstanceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|Server")
	FName SpeciesId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|Server")
	TObjectPtr<UUEPokemonSpeciesData> SpeciesData = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|Server")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|Server")
	float CurrentHP = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Pokemon|Server")
	float MaxHP = 0.0f;
};

USTRUCT()
struct FUEPokemonServerPlayerState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FUEPokemonServerOwnedPokemon> OwnedPokemons;

	UPROPERTY()
	int32 ActiveRuntimePokemonId = 0;
};

USTRUCT()
struct FUEPokemonServerRuntimePokemon
{
	GENERATED_BODY()

	UPROPERTY()
	int32 OwnerServerPlayerId = 0;

	UPROPERTY()
	int32 RuntimePokemonId = 0;

	UPROPERTY()
	int32 PokemonInstanceId = 0;

	UPROPERTY()
	FName SpeciesId = NAME_None;

	UPROPERTY()
	TObjectPtr<UUEPokemonSpeciesData> SpeciesData = nullptr;

	UPROPERTY()
	int32 Level = 1;

	UPROPERTY()
	float CurrentHP = 0.0f;

	UPROPERTY()
	float MaxHP = 0.0f;
};

UCLASS()
class HEAVENHYPERVOICE_API UUEPokemonServerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Pokemon|Server|Event")
	FUEPokemonOwnedRosterChangedSignature OnOwnedPokemonRosterChanged;

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void RegisterOwnedPokemons(int32 OwnerServerPlayerId, const TArray<FUEPokemonServerOwnedPokemon>& OwnedPokemons);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void ClearOwnedPokemons(int32 OwnerServerPlayerId);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	FUEPokemonServerSpawnResponse RequestSpawnPokemon(int32 OwnerServerPlayerId, int32 PokemonInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	bool RequestDespawnPokemon(int32 OwnerServerPlayerId, int32 RuntimePokemonId);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	bool HasActivePokemon(int32 OwnerServerPlayerId) const;

	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	bool TryGetOwnedPokemon(int32 OwnerServerPlayerId, int32 PokemonInstanceId, FUEPokemonServerOwnedPokemon& OutOwnedPokemon) const;

	// UI에는 내부 배열 주소를 넘기지 않고 복사본을 반환해 서버 런타임 상태를 보호한다.
	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	TArray<FUEPokemonServerOwnedPokemon> GetOwnedPokemons(int32 OwnerServerPlayerId) const;

private:
	FUEPokemonServerOwnedPokemon NormalizeOwnedPokemon(const FUEPokemonServerOwnedPokemon& OwnedPokemon, int32& NextGeneratedInstanceId) const;
	const FUEPokemonServerOwnedPokemon* FindOwnedPokemon(int32 OwnerServerPlayerId, int32 PokemonInstanceId) const;
	FUEPokemonServerOwnedPokemon* FindMutableOwnedPokemon(int32 OwnerServerPlayerId, int32 PokemonInstanceId);
	float ResolveMaxHP(const FUEPokemonServerOwnedPokemon& OwnedPokemon) const;

	UPROPERTY(Transient)
	TMap<int32, FUEPokemonServerPlayerState> PlayerStates;

	UPROPERTY(Transient)
	TMap<int32, FUEPokemonServerRuntimePokemon> RuntimePokemons;

	int32 NextRuntimePokemonId = 1;
};
