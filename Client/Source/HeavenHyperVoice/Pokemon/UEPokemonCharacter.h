// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UEPokemonCharacter.generated.h"

class UUEPokemonServerComponent;
class UUEPokemonSpeciesData;

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
	Fainted,
	FieldAnimationStarted
};

// 야생 포켓몬이 필드에서 쉬는 동안 사용할 수 있는 비전투 행동 목록이다.
// Start / Loop / End로 나뉜 행동은 AnimInstance가 이 값 하나를 받아 순서대로 재생한다.
UENUM(BlueprintType)
enum class EUEPokemonFieldAnimation : uint8
{
	None,
	Idle01,
	Idle02,
	TurnLeft90,
	TurnRight90,
	Eat01,
	Eat02,
	Sleep,
	Rest,
	Notice,
	Roar,
	Glad,
	Hate,
	Refresh,
	StepOut
};

// 플레이어의 기술 슬롯 명령을 종별 DataAsset의 실제 공격 시퀀스로 바꾸기 위한 종류다.
// 숫자키 슬롯과 애니메이션을 분리해 두면 나중에 STT 기술 명령도 같은 값을 사용할 수 있다.
UENUM(BlueprintType)
enum class EUEPokemonAttackAnimation : uint8
{
	None,
	Attack01,
	Attack02,
	RangeAttack01,
	RangeAttack02
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

	// FieldAnimationStarted 이벤트일 때 재생할 종별 필드 행동이다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	EUEPokemonFieldAnimation FieldAnimation = EUEPokemonFieldAnimation::None;

	// 먹기, 잠자기, 휴식처럼 Loop 구간이 있는 행동의 반복 횟수다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server", meta = (ClampMin = "1"))
	int32 FieldAnimationLoopCount = 1;

	// AttackStarted 이벤트일 때 재생할 종별 공격 애니메이션이다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	EUEPokemonAttackAnimation AttackAnimation = EUEPokemonAttackAnimation::None;

	// Start / Loop / End 구조인 원거리 공격의 Loop 반복 횟수다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server", meta = (ClampMin = "1"))
	int32 AttackAnimationLoopCount = 1;

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

	// 서버 야생 포켓몬용. 종족 번호만 주고 큐브를 그 종족 색으로 칠한다.
	// 스켈레탈 메시 없이 뜨는 임시 표현이라 데이터 애셋이 필요 없다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Species")
	void SetWildSpecies(int32 SpeciesNumber);

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

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Pokemon|Animation", meta = (DisplayName = "On Server Animation Event"))
	void BP_OnServerAnimationEvent(EUEPokemonAnimationEvent AnimationEvent, const FUEPokemonServerMoveSnapshot& Snapshot);

private:
	void ApplyPokemonSpeciesData();

	// 실제 스켈레탈 메시가 없을 때 큐브를 종족 대표 색으로 칠한다.
	void ApplyDebugAppearance();
	void ApplyServerAnimationSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot);
	void UpdateServerDrivenMovement(float DeltaSeconds);
	void ConfigureServerDrivenMovement();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonServerComponent> ServerComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Species", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonSpeciesData> PokemonSpeciesData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerLocationInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerRotationInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerHardSnapDistance = 300.0f;

	UPROPERTY(Transient)
	bool bDebugAppearanceApplied = false;

	FVector TargetServerLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector TargetServerVelocity = FVector::ZeroVector;

	UPROPERTY(Transient)
	FRotator TargetServerRotation = FRotator::ZeroRotator;

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
