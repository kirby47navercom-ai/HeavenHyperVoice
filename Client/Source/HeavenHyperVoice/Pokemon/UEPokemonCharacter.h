// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UEPokemonCharacter.generated.h"

class UUEPokemonTestServerComponent;

UENUM(BlueprintType)
enum class EUEPokemonAnimationState : uint8
{
	Idle,
	Moving,
	Turning,
	Spawning,
	Despawning,
	Attacking,
	HitReact,
	Fainted
};

UENUM(BlueprintType)
enum class EUEPokemonAnimationEvent : uint8
{
	None,
	SpawnStarted,
	SpawnCompleted,
	DespawnStarted,
	DespawnCompleted,
	AttackStarted,
	HitReact,
	Fainted
};

USTRUCT(BlueprintType)
struct FUEPokemonServerMoveSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	int32 PokemonId = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	bool bTeleported = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	EUEPokemonAnimationState AnimationState = EUEPokemonAnimationState::Idle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	EUEPokemonAnimationEvent AnimationEvent = EUEPokemonAnimationEvent::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	float ServerTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	float EventDurationSeconds = 0.0f;
};

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEPokemonCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AUEPokemonCharacter();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void ApplyServerMoveSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void ApplyServerMoveTarget(const FVector& ServerLocation, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bTeleported);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	EUEPokemonAnimationState GetServerAnimationState() const { return ServerAnimationState; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	EUEPokemonAnimationEvent GetLastServerAnimationEvent() const { return LastServerAnimationEvent; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	float GetLastServerAnimationEventTimeSeconds() const { return LastServerAnimationEventTimeSeconds; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	float GetLastServerAnimationEventDurationSeconds() const { return LastServerAnimationEventDurationSeconds; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Test Server")
	UUEPokemonTestServerComponent* GetTestServerComponent() const { return TestServerComponent; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Pokemon|Animation", meta = (DisplayName = "On Server Animation Event"))
	void BP_OnServerAnimationEvent(EUEPokemonAnimationEvent AnimationEvent, const FUEPokemonServerMoveSnapshot& Snapshot);

private:
	void ApplyServerAnimationSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot);
	void UpdateServerDrivenMovement(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pokemon|Test Server", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonTestServerComponent> TestServerComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerLocationInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerRotationInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerHardSnapDistance = 300.0f;

	UPROPERTY(Transient)
	FVector TargetServerLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector TargetServerVelocity = FVector::ZeroVector;

	UPROPERTY(Transient)
	FRotator TargetServerRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Animation", meta = (AllowPrivateAccess = "true"))
	EUEPokemonAnimationState ServerAnimationState = EUEPokemonAnimationState::Idle;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Animation", meta = (AllowPrivateAccess = "true"))
	EUEPokemonAnimationEvent LastServerAnimationEvent = EUEPokemonAnimationEvent::None;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Animation", meta = (AllowPrivateAccess = "true"))
	float LastServerAnimationEventTimeSeconds = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Animation", meta = (AllowPrivateAccess = "true"))
	float LastServerAnimationEventDurationSeconds = 0.0f;

	bool bHasServerMoveTarget = false;
};
