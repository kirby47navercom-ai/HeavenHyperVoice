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

	// 플레이어가 선택한 기술 슬롯의 공격 모션을 서버 상태로 승인하고 모든 클라이언트가 받을 스냅샷을 만든다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	bool SendServerAttackCommand(EUEPokemonAttackAnimation AttackAnimation, int32 LoopCount = 1);

private:
	void TryLoadServerMap();
	void RunServerSimulationTick(float DeltaSeconds);
	void RunFollowSimulationTick(AUEPokemonCharacter& PokemonCharacter, float DeltaSeconds);
	void RunWanderSimulationTick(AUEPokemonCharacter& PokemonCharacter, float DeltaSeconds);
	void ApplyServerCommand(AUEPokemonCharacter& PokemonCharacter, const HHV::PokemonAI::Command& Command, float DeltaSeconds);
	void SendServerSnapshot(AUEPokemonCharacter& PokemonCharacter, const FVector& Location, const FVector& Velocity, const FRotator& Rotation, bool bTeleported, EUEPokemonAnimationState AnimationState, EUEPokemonAnimationEvent AnimationEvent, float EventDurationSeconds, EUEPokemonFieldAnimation FieldAnimation = EUEPokemonFieldAnimation::None, int32 FieldAnimationLoopCount = 1, EUEPokemonAttackAnimation AttackAnimation = EUEPokemonAttackAnimation::None, int32 AttackAnimationLoopCount = 1) const;
	EUEPokemonAnimationState ResolveAnimationState(EUEPokemonAnimationState FallbackState);
	float GetServerTimeSeconds() const;
	float ResolveServerMoveSpeed(const AUEPokemonCharacter& PokemonCharacter) const;
	void SendIdleSnapshot(AUEPokemonCharacter& PokemonCharacter);
	bool TryStartRandomFieldAnimation(AUEPokemonCharacter& PokemonCharacter);
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

	// 종족 DA 의 MoveSpeed 를 언리얼 uu/s 로 바꾸는 배율. DA 값이 게임 스탯
	// 스케일(랄토스 25 ~ 자망칼 114)이라 그대로는 너무 느리다.
	// 3.0 이면 파이리 101 -> 303 으로, 플레이어 걷기(260)를 조금 넘는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float SpeciesMoveSpeedToUnrealUnits = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	EUEPokemonServerSimulationMode SimulationMode = EUEPokemonServerSimulationMode::FollowOwner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WanderWaitSeconds = 3.0f;

	// 매번 같은 시간 동안 멈춰 있지 않도록 기본 대기 시간에 더하고 빼는 범위다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WanderWaitVariationSeconds = 2.0f;

	// 한 번 목적지를 정할 때 현재 위치에서 이동할 최소 및 최대 거리다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WanderMinDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WanderMaxDistance = 700.0f;

	// 야생 포켓몬이 최초 생성 위치에서 지나치게 멀어지지 않게 하는 활동 반경이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WanderHomeRadius = 1200.0f;

	// 산책마다 종별 MoveSpeed에 곱할 최소 및 최대 배율이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", ClampMax = "2.0"))
	float WanderMinMoveSpeedScale = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", ClampMax = "2.0"))
	float WanderMaxMoveSpeedScale = 1.0f;

	// 목적지에서 쉬기 시작할 때 임의의 방향을 한 번 바라볼 확률이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float WanderLookAroundChance = 0.65f;

	// 목적지에서 쉬기 시작할 때 종별 필드 행동 애니메이션을 재생할 확률이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float WanderFieldAnimationChance = 0.75f;

	// 먹기, 잠자기, 휴식처럼 Loop가 있는 행동에서 반복할 최소 및 최대 횟수다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "10"))
	int32 FieldAnimationMinLoopCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "10"))
	int32 FieldAnimationMaxLoopCount = 2;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	FString DefaultServerMapFileName;

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
	bool bIsFieldAnimationActive = false;
	bool bIsAttackAnimationActive = false;
	float ServerTickAccumulator = 0.0f;
	float ForcedAnimationStateEndServerTimeSeconds = 0.0f;
	float FieldAnimationEndServerTimeSeconds = 0.0f;
	float AttackAnimationEndServerTimeSeconds = 0.0f;

	// 등록된 필드 행동을 한 번씩 모두 사용한 뒤 다시 채우는 셔플 주머니다.
	// 완전 무작위에서 특정 행동이 오랫동안 나오지 않는 문제를 막는다.
	TArray<EUEPokemonFieldAnimation> FieldAnimationBag;
	FRandomStream FieldAnimationRandomStream;
};
