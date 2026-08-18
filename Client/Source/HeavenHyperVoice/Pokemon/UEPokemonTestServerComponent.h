// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "AI/Own/PokemonFSM.h"
#include "../Map/HHVServerMapRuntime.h"

#include "UEPokemonTestServerComponent.generated.h"

class AUEPokemonCharacter;

UCLASS(ClassGroup = (Pokemon), meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEPokemonTestServerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEPokemonTestServerComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Test Server")
	void SetFollowTargetActor(AActor* NewFollowTargetActor);

private:
	void TryLoadServerMap();
	void RunServerSimulationTick(float DeltaSeconds);
	void ApplyServerCommand(AUEPokemonCharacter& PokemonCharacter, const HHV::PokemonAI::Command& Command, float DeltaSeconds);
	void SendServerSnapshot(AUEPokemonCharacter& PokemonCharacter, const FVector& Location, const FVector& Velocity, const FRotator& Rotation, bool bTeleported) const;

	AUEPokemonCharacter* GetPokemonOwner() const;
	AActor* ResolveFollowTargetActor() const;
	HHV::PokemonAI::CompanionContext MakeCompanionContext(const AUEPokemonCharacter& PokemonCharacter, const AActor& CurrentFollowTargetActor, float DeltaSeconds) const;
	HHV::Map::AgentSettings MakeAgentSettings(const AUEPokemonCharacter& PokemonCharacter) const;
	FString ResolveServerMapFilePath() const;
	float GetFollowTargetYawDegrees(const AActor& CurrentFollowTargetActor) const;

	static HHV::Map::Vec3 ToServerVec3(const FVector& Vector);
	static FVector ToUnrealVector(const HHV::Map::Vec3& Vector);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Test Server", meta = (AllowPrivateAccess = "true"))
	bool bEnableTestServer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Test Server", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float ServerTickRate = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Test Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerMoveSpeed = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Test Server", meta = (AllowPrivateAccess = "true"))
	bool bTryLoadDefaultServerMap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Test Server", meta = (AllowPrivateAccess = "true"))
	FString ServerMapFilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Test Server", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> FollowTargetActor = nullptr;

	UPROPERTY(Transient)
	FVector ServerSimulatedLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector ServerSimulatedVelocity = FVector::ZeroVector;

	UPROPERTY(Transient)
	FRotator ServerSimulatedRotation = FRotator::ZeroRotator;

	HHV::PokemonAI::PokemonFSM TestServerBrain;
	HHV::Map::ServerMapRuntime ServerMapRuntime;
	bool bServerMapLoaded = false;
	float ServerTickAccumulator = 0.0f;
};
