// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "../AI/Own/PokemonFSM.h"
#include "../AI/Wild/PokemonWildFSM.h"
#include "../UEPokemonCharacter.h"
#include "../../Map/HHVServerMapRuntime.h"

#include "UEPokemonServerComponent.generated.h"

class AUEPokemonCharacter;

UCLASS(ClassGroup = (Pokemon), meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEPokemonServerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEPokemonServerComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void SetFollowTargetActor(AActor* NewFollowTargetActor);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void SetServerSimulationEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	bool IsServerSimulationEnabled() const { return bEnableServer; }

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void SetServerSimulationMode(EUEPokemonServerSimulationMode NewSimulationMode);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	EUEPokemonServerSimulationMode GetServerSimulationMode() const { return SimulationMode; }

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void InitializeServerRuntimePokemon(int32 RuntimePokemonId, int32 PokemonInstanceId, float CurrentHP, float MaxHP);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	int32 GetServerPokemonId() const { return ServerPokemonId; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	int32 GetServerPokemonInstanceId() const { return ServerPokemonInstanceId; }

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void SendServerAnimationEvent(EUEPokemonAnimationEvent AnimationEvent, EUEPokemonAnimationState AnimationState, float EventDurationSeconds = 0.0f);

private:
	void TryLoadServerMap();
	void RunServerSimulationTick(float DeltaSeconds);
	void RunFollowSimulationTick(AUEPokemonCharacter& PokemonCharacter, float DeltaSeconds);
	void RunWanderSimulationTick(AUEPokemonCharacter& PokemonCharacter, float DeltaSeconds);
	void ApplyServerCommand(AUEPokemonCharacter& PokemonCharacter, const HHV::PokemonAI::Command& Command, float DeltaSeconds);
	void SendServerSnapshot(AUEPokemonCharacter& PokemonCharacter, const FVector& Location, const FVector& Velocity, const FRotator& Rotation, bool bTeleported, EUEPokemonAnimationState AnimationState, EUEPokemonAnimationEvent AnimationEvent, float EventDurationSeconds) const;
	EUEPokemonAnimationState ResolveAnimationState(EUEPokemonAnimationState FallbackState);
	float GetServerTimeSeconds() const;
	float ResolveServerMoveSpeed(const AUEPokemonCharacter& PokemonCharacter) const;
	void SendIdleSnapshot(AUEPokemonCharacter& PokemonCharacter);
	void ResetWildState();

	AUEPokemonCharacter* GetPokemonOwner() const;
	AActor* ResolveFollowTargetActor() const;
	HHV::PokemonAI::OwnContext MakeOwnContext(const AUEPokemonCharacter& PokemonCharacter, const AActor& CurrentFollowTargetActor, float DeltaSeconds) const;
	HHV::PokemonAI::WildContext MakeWildContext(const AUEPokemonCharacter& PokemonCharacter, float DeltaSeconds) const;
	HHV::Map::AgentSettings MakeAgentSettings(const AUEPokemonCharacter& PokemonCharacter) const;
	FString ResolveServerMapFilePath() const;
	float GetFollowTargetYawDegrees(const AActor& CurrentFollowTargetActor) const;

	static HHV::Map::Vec3 ToServerVec3(const FVector& Vector);
	static FVector ToUnrealVector(const HHV::Map::Vec3& Vector);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	bool bEnableServer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float ServerTickRate = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerMoveSpeed = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	EUEPokemonServerSimulationMode SimulationMode = EUEPokemonServerSimulationMode::FollowOwner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WanderWaitSeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float WanderAcceptanceRadius = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 WanderSearchAttempts = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	int32 WanderRandomSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	bool bTryLoadDefaultServerMap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	FString ServerMapFilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> FollowTargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 ServerPokemonId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 ServerPokemonInstanceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerCurrentHP = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerMaxHP = 0.0f;

	UPROPERTY(Transient)
	FVector ServerSimulatedLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector ServerSimulatedVelocity = FVector::ZeroVector;

	UPROPERTY(Transient)
	FRotator ServerSimulatedRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	EUEPokemonAnimationState ForcedAnimationState = EUEPokemonAnimationState::Idle;

	HHV::PokemonAI::PokemonFSM ServerBrain;
	HHV::PokemonAI::PokemonWildFSM WildBrain;
	HHV::Map::ServerMapRuntime ServerMapRuntime;
	bool bServerMapLoaded = false;
	bool bHasForcedAnimationState = false;
	float ServerTickAccumulator = 0.0f;
	float ForcedAnimationStateEndServerTimeSeconds = 0.0f;
};
