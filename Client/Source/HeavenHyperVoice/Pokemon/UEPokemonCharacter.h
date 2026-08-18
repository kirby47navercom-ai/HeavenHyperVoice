// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UEPokemonCharacter.generated.h"

class UUEPokemonTestServerComponent;
class UUEPokemonServerComponent;
class UUEPokemonSpeciesData;
class UUEPokemonAnimationDataAsset;
class UAnimSequence;

// 서버와 월드 서브시스템이 공유하는 포켓몬 상태 열거형이다.
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

UENUM(BlueprintType)
enum class EUEPokemonRenderType : uint8
{
	Wild,
	Own,
	Other,
	Boss
};

UENUM(BlueprintType)
enum class EUEPokemonServerSimulationMode : uint8
{
	FollowOwner,
	Wander
};

USTRUCT(BlueprintType)
struct FUEPokemonServerMoveSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	int32 PokemonId = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	int32 PokemonInstanceId = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	FName SpeciesId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	EUEPokemonRenderType RenderType = EUEPokemonRenderType::Wild;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	float CurrentHP = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	float MaxHP = 0.0f;

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

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void ApplyServerStats(float ServerCurrentHP, float ServerMaxHP);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	int32 GetServerPokemonId() const { return ServerPokemonId; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	int32 GetPokemonInstanceId() const { return PokemonInstanceId; }

	UFUNCTION(BlueprintCallable, Category = "Pokemon|World")
	void SetRenderType(EUEPokemonRenderType NewRenderType);

	UFUNCTION(BlueprintPure, Category = "Pokemon|World")
	EUEPokemonRenderType GetRenderType() const { return RenderType; }

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Species")
	void SetPokemonSpeciesData(UUEPokemonSpeciesData* NewSpeciesData);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Species")
	UUEPokemonSpeciesData* GetPokemonSpeciesData() const { return PokemonSpeciesData; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Species")
	FName GetPokemonSpeciesId() const;

	UFUNCTION(BlueprintPure, Category = "Pokemon|Stats")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Stats")
	float GetMaxHP() const { return MaxHP; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Movement")
	float GetConfiguredMoveSpeed() const { return ConfiguredMoveSpeed; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	EUEPokemonAnimationState GetServerAnimationState() const { return ServerAnimationState; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	EUEPokemonAnimationEvent GetLastServerAnimationEvent() const { return LastServerAnimationEvent; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	float GetLastServerAnimationEventTimeSeconds() const { return LastServerAnimationEventTimeSeconds; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	float GetLastServerAnimationEventDurationSeconds() const { return LastServerAnimationEventDurationSeconds; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	UUEPokemonServerComponent* GetServerComponent() const { return ServerComponent; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Test Server")
	UUEPokemonTestServerComponent* GetTestServerComponent() const { return TestServerComponent; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	UUEPokemonAnimationDataAsset* GetPokemonAnimationData() const { return PokemonAnimationData; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Pokemon|Animation", meta = (DisplayName = "On Server Animation Event"))
	void BP_OnServerAnimationEvent(EUEPokemonAnimationEvent AnimationEvent, const FUEPokemonServerMoveSnapshot& Snapshot);

private:
	void ApplyPokemonSpeciesData();
	void ApplyServerAnimationSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot);
	void UpdateServerDrivenMovement(float DeltaSeconds);
	void UpdatePokemonAnimation();
	void PlayPokemonAnimation(UAnimSequence* Sequence, bool bLoop);
	void ConfigureServerDrivenMovement();

	// 서버 동기화 컴포넌트는 풀 받은 서버 경로를 담당하고, 기존 테스트 컴포넌트는 그대로 유지한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonServerComponent> ServerComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pokemon|Test Server", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonTestServerComponent> TestServerComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Species", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonSpeciesData> PokemonSpeciesData = nullptr;

	// 포켓몬 애님 BP가 종별 애니메이션을 꺼내 쓸 때 참조하는 데이터 에셋이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonAnimationDataAsset> PokemonAnimationData = nullptr;

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

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentPokemonAnimation = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Stats", meta = (AllowPrivateAccess = "true"))
	float CurrentHP = 100.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Stats", meta = (AllowPrivateAccess = "true"))
	float MaxHP = 100.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Movement", meta = (AllowPrivateAccess = "true"))
	float ConfiguredMoveSpeed = 280.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	int32 ServerPokemonId = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	int32 PokemonInstanceId = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	FName ServerSpeciesId = NAME_None;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|World", meta = (AllowPrivateAccess = "true"))
	EUEPokemonRenderType RenderType = EUEPokemonRenderType::Wild;

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
