// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "UEPokemonCharacter.generated.h"

class AUEPokemonCharacter;
class UAbilitySystemComponent;
class UUEAbilitySystemComponent;
class UUEPokemonAttributeSet;
class UUEPokemonSpeciesCatalog;
class UUEPokemonSpeciesData;
class USoundBase;
struct FOnAttributeChangeData;

// 블루프린트 UI와 피격 연출이 포켓몬 체력 변경을 즉시 구독할 때 사용한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FUEPokemonHealthChangedSignature,
	AUEPokemonCharacter*, Pokemon,
	float, OldHealth,
	float, NewHealth,
	float, MaxHealth);

// 체력이 처음 0이 되는 순간 한 번만 방송되는 기절 델리게이트다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FUEPokemonFaintedSignature,
	AUEPokemonCharacter*, Pokemon);

// GameplayTag로 요청한 어빌리티가 실제 활성화됐을 때 방송한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FUEPokemonAbilityActivatedSignature,
	AUEPokemonCharacter*, Pokemon,
	FGameplayTag, AbilityTag);

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
class HEAVENHYPERVOICE_API AUEPokemonCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AUEPokemonCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Pokemon|GAS")
	UUEAbilitySystemComponent* GetPokemonAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|GAS")
	UUEPokemonAttributeSet* GetPokemonAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void ApplyServerMoveSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void InitializeServerEntity(int64 NewServerEntityId, int32 SpeciesNumber, EUEPokemonRenderType NewRenderType);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void ApplyServerMoveTarget(const FVector& ServerLocation, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bTeleported);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void ApplyServerStats(float ServerCurrentHP, float ServerMaxHP);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	int32 GetServerPokemonId() const { return ServerPokemonId; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Server")
	int64 GetServerEntityId() const { return ServerEntityId; }

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
	float GetCurrentHP() const;

	UFUNCTION(BlueprintPure, Category = "Pokemon|Stats")
	float GetMaxHP() const;

	UFUNCTION(BlueprintPure, Category = "Pokemon|GAS|Attribute")
	float GetAttackPower() const;

	UFUNCTION(BlueprintPure, Category = "Pokemon|GAS|Attribute")
	float GetDefense() const;

	// DamageAmount는 방어 계산까지 끝난 최종 피해량이다. 기술별 방어 공식은 GameplayEffect에서 처리한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|GAS|Health", meta = (ClampMin = "0.0"))
	float ApplyPokemonDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|GAS|Health", meta = (ClampMin = "0.0"))
	float RestorePokemonHealth(float HealAmount);

	// 태그와 일치하는 시작 어빌리티를 실행하고 성공 여부를 반환한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|GAS|Ability")
	bool ActivatePokemonAbilityByTag(FGameplayTag AbilityTag);

	UPROPERTY(BlueprintAssignable, Category = "Pokemon|GAS|Event")
	FUEPokemonHealthChangedSignature OnPokemonHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Pokemon|GAS|Event")
	FUEPokemonFaintedSignature OnPokemonFainted;

	UPROPERTY(BlueprintAssignable, Category = "Pokemon|GAS|Event")
	FUEPokemonAbilityActivatedSignature OnPokemonAbilityActivated;

	UFUNCTION(BlueprintPure, Category = "Pokemon|Movement")
	float GetConfiguredMoveSpeed() const { return ConfiguredMoveSpeed; }

	/**
	 * 목표점이 이만큼 멀어지면 보간하지 않고 붙여 놓는다.
	 *
	 * 서버 스냅샷을 받는 야생 포켓몬에게는 300 정도가 맞다 — 그 이상 벌어졌다는
	 * 것은 패킷이 밀렸다는 뜻이라 따라가는 것보다 맞추는 편이 낫다. 주인을 쫓는
	 * 파트너는 정상적으로 뒤처지는 일이 잦아서 훨씬 큰 값을 쓴다.
	 */
	void SetServerHardSnapDistance(float InDistance) { ServerHardSnapDistance = InDistance; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	EUEPokemonAnimationState GetServerAnimationState() const { return ServerAnimationState; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	EUEPokemonAnimationEvent GetLastServerAnimationEvent() const { return LastServerAnimationEvent; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	float GetLastServerAnimationEventTimeSeconds() const { return LastServerAnimationEventTimeSeconds; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	float GetLastServerAnimationEventDurationSeconds() const { return LastServerAnimationEventDurationSeconds; }

	// 소환 후보 중 하나를 무작위로 재생한다. 후보가 없으면 대표 울음을 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Audio")
	void PlaySummonCry();

	// 기절 후보 중 하나를 무작위로 재생한다. 일반 울음을 임의로 대신 쓰지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Audio")
	void PlayFaintCry();

	// 현재 종족 DataAsset의 야생 울음 후보 중 하나를 무작위로 골라 재생한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Audio")
	void PlayRandomWildCry();

	// 블루프린트 상태 머신이나 상호작용 연출에서 기쁨 울음을 요청할 때 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Audio")
	void PlayRandomHappyCry();

	// 적 발견·전투 진입 연출에서 분노 울음을 요청할 때 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Audio")
	void PlayRandomAngryCry();

	// 피격·실패 연출에서 슬픔 울음을 요청할 때 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Audio")
	void PlayRandomSadCry();

	// 물리 기술 애니메이션과 함께 짧은 공격 울음을 요청할 때 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Audio")
	void PlayRandomPhysicalAttackCry();

	// 특수 기술 애니메이션과 함께 속성 공격 울음을 요청할 때 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Audio")
	void PlayRandomSpecialAttackCry();

	// 컷신이나 종별 고유 연출에서 특수음성 후보를 요청할 때 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Audio")
	void PlayRandomSpecialCry();

	// 휴식·주변 관찰 같은 필드 행동에서 환경 반응 울음을 요청할 때 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Audio")
	void PlayRandomAmbientCry();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Pokemon|Animation", meta = (DisplayName = "On Server Animation Event"))
	void BP_OnServerAnimationEvent(EUEPokemonAnimationEvent AnimationEvent, const FUEPokemonServerMoveSnapshot& Snapshot);

private:
	void ApplyPokemonSpeciesData();
	void InitializeAbilitySystem();
	void InitializePokemonAttributes(float NewCurrentHealth, float NewMaxHealth, float NewAttackPower, float NewDefense);
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);
	float SetPokemonHealth(float NewHealth);

	// 실제 스켈레탈 메시가 없을 때 큐브를 종족 대표 색으로 칠한다.
	void ApplyDebugAppearance();
	void ApplyServerAnimationSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot);
	void UpdateServerDrivenMovement(float DeltaSeconds);
	void ConfigureServerDrivenMovement();
	void RefreshWildCryTimer();
	void HandleWildCryTimer();
	USoundBase* SelectRandomCry(const TArray<TObjectPtr<USoundBase>>& CryCandidates, USoundBase* FallbackCry = nullptr) const;
	void PlayCrySound(USoundBase* CrySound) const;

	// 포켓몬 자신이 ASC의 OwnerActor와 AvatarActor를 함께 맡는 단순한 구조다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pokemon|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEAbilitySystemComponent> AbilitySystemComponent = nullptr;

	// ASC가 관리하는 포켓몬 공통 전투 수치 묶음이다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pokemon|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonAttributeSet> AttributeSet = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Species", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonSpeciesData> PokemonSpeciesData = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Species", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonSpeciesCatalog> PokemonSpeciesCatalog = nullptr;

	// 필드 서버는 20Hz로 좌표를 보낸다. 클라이언트는 이 시간 동안 이전 좌표와
	// 새 좌표를 일정한 속도로 연결해 프레임 사이의 떨림을 없앤다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", ClampMax = "0.2"))
	float ServerSnapshotIntervalSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerHardSnapDistance = 300.0f;

	UPROPERTY(Transient)
	bool bDebugAppearanceApplied = false;

	FVector TargetServerLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector ServerMoveStartLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FRotator TargetServerRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	FRotator ServerMoveStartRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	float ServerMoveElapsedSeconds = 0.0f;

	UPROPERTY(Transient)
	float ServerMoveDurationSeconds = 0.05f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Stats", meta = (AllowPrivateAccess = "true"))
	float CurrentHP = 100.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Stats", meta = (AllowPrivateAccess = "true"))
	float MaxHP = 100.0f;

	UPROPERTY(Transient)
	bool bAbilitySystemInitialized = false;

	UPROPERTY(Transient)
	bool bFaintDelegateBroadcast = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Movement", meta = (AllowPrivateAccess = "true"))
	float ConfiguredMoveSpeed = 280.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	int32 ServerPokemonId = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true"))
	int64 ServerEntityId = 0;

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

	// 고정 주기 반복 타이머가 아니라 매번 새 간격을 뽑는 한 번짜리 타이머로 사용한다.
	FTimerHandle WildCryTimerHandle;

	bool bHasServerMoveTarget = false;
};
