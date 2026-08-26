#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UEPokemonSpeciesData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;
class UAnimSequence;
class UGameplayAbility;
class UTexture2D;


// ============================================================================
// Pokemon Skill Data
// ============================================================================

USTRUCT(BlueprintType)
struct FUEPokemonSkillData
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FName SkillId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	float Power = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	float Range = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	float CooldownSeconds = 0.0f;
};


// ============================================================================
// Pokemon Species Data
// ============================================================================

UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEPokemonSpeciesData : public UDataAsset
{
	GENERATED_BODY()

public:

	// ========================================================================
	// Identity
	// ========================================================================

	/**
	 * 포켓몬 종류 식별자.
	 *
	 * 예:
	 * Bulbasaur
	 * Charmander
	 * Eevee
	 *
	 * 서버와 Species를 구분할 때도 사용할 수 있습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Identity")
	FName SpeciesId = NAME_None;

	// HUD와 도감에 표시할 이름이다. 비어 있으면 UI가 SpeciesId를 대신 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Profile")
	FText DisplayName;

	// 포켓몬 프로필에 사용할 초상화다. 경로 문자열이 아니라 DataAsset 변수로 직접 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Profile")
	TObjectPtr<UTexture2D> ProfileIcon = nullptr;


	// ========================================================================
	// Visual
	// ========================================================================

	/** 이 포켓몬이 사용할 Skeletal Mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Visual")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	// 스켈레탈 메시가 없을 때 큐브를 칠할 색. 알파가 0이면 종족 기본색을 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Visual")
	FLinearColor DebugColor = FLinearColor(0.f, 0.f, 0.f, 0.f);

	/** 이 포켓몬이 사용할 Animation Blueprint */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Visual")
	TSubclassOf<UAnimInstance> AnimInstanceClass;

	/** 캐릭터 Capsule을 기준으로 Mesh 위치/회전/크기 보정 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Visual")
	FTransform MeshRelativeTransform = FTransform::Identity;


	// ========================================================================
	// Spawn / Despawn
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Spawn")
	TObjectPtr<UAnimMontage> SpawnMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Spawn")
	TObjectPtr<UAnimMontage> DespawnMontage = nullptr;


	// ========================================================================
	// Animation - Idle
	// ========================================================================

	/** defaultwait01_loop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Idle")
	TObjectPtr<UAnimSequence> Idle = nullptr;

	/** defaultidle01 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Idle")
	TObjectPtr<UAnimSequence> Idle01 = nullptr;

	/** defaultidle02 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Idle")
	TObjectPtr<UAnimSequence> Idle02 = nullptr;

	/** battlewait01_loop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Idle")
	TObjectPtr<UAnimSequence> BattleIdle = nullptr;


	// ========================================================================
	// Animation - Locomotion
	// ========================================================================

	/** walk01_loop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Locomotion")
	TObjectPtr<UAnimSequence> Walk = nullptr;

	/** run01_loop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Locomotion")
	TObjectPtr<UAnimSequence> Run = nullptr;

	/** turn_l090 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Locomotion")
	TObjectPtr<UAnimSequence> TurnLeft90 = nullptr;

	/** turn_r090 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Locomotion")
	TObjectPtr<UAnimSequence> TurnRight90 = nullptr;


	// ========================================================================
	// Animation - Jump / Fall
	// ========================================================================

	/** jumpup01_start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Jump")
	TObjectPtr<UAnimSequence> JumpStart = nullptr;

	/** jumpup01_loop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Jump")
	TObjectPtr<UAnimSequence> JumpLoop = nullptr;

	/** jumpdown01_start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Jump")
	TObjectPtr<UAnimSequence> FallStart = nullptr;

	/** jumpdown01_loop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Jump")
	TObjectPtr<UAnimSequence> FallLoop = nullptr;

	/** land02 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Jump")
	TObjectPtr<UAnimSequence> Land = nullptr;


	// ========================================================================
	// Animation - Combat
	// ========================================================================

	/** attack01 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Combat")
	TObjectPtr<UAnimSequence> Attack01 = nullptr;

	/** attack02 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Combat")
	TObjectPtr<UAnimSequence> Attack02 = nullptr;

	/** rangeattack01 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Combat")
	TObjectPtr<UAnimSequence> RangeAttack01 = nullptr;

	/** rangeattack02_start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Combat")
	TObjectPtr<UAnimSequence> RangeAttack02Start = nullptr;

	/** rangeattack02_loop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Combat")
	TObjectPtr<UAnimSequence> RangeAttack02Loop = nullptr;

	/** rangeattack02_end */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Combat")
	TObjectPtr<UAnimSequence> RangeAttack02End = nullptr;


	// ========================================================================
	// Animation - Damage
	// ========================================================================

	/** damage01 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Damage")
	TObjectPtr<UAnimSequence> Damage01 = nullptr;

	/** damage02 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Damage")
	TObjectPtr<UAnimSequence> Damage02 = nullptr;


	// ========================================================================
	// Animation - Stun
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Status|Stun")
	TObjectPtr<UAnimSequence> StunStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Status|Stun")
	TObjectPtr<UAnimSequence> StunLoop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Status|Stun")
	TObjectPtr<UAnimSequence> StunEnd = nullptr;


	// ========================================================================
	// Animation - Down
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Status|Down")
	TObjectPtr<UAnimSequence> DownStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Status|Down")
	TObjectPtr<UAnimSequence> DownLoop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Status|Down")
	TObjectPtr<UAnimSequence> DownEnd = nullptr;


	// ========================================================================
	// Animation - Eat 01
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Eat01")
	TObjectPtr<UAnimSequence> Eat01Start = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Eat01")
	TObjectPtr<UAnimSequence> Eat01Loop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Eat01")
	TObjectPtr<UAnimSequence> Eat01End = nullptr;


	// ========================================================================
	// Animation - Eat 02
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Eat02")
	TObjectPtr<UAnimSequence> Eat02Start = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Eat02")
	TObjectPtr<UAnimSequence> Eat02Loop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Eat02")
	TObjectPtr<UAnimSequence> Eat02End = nullptr;


	// ========================================================================
	// Animation - Sleep
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Sleep")
	TObjectPtr<UAnimSequence> SleepStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Sleep")
	TObjectPtr<UAnimSequence> SleepLoop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Sleep")
	TObjectPtr<UAnimSequence> SleepEnd = nullptr;


	// ========================================================================
	// Animation - Rest
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Rest")
	TObjectPtr<UAnimSequence> RestStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Rest")
	TObjectPtr<UAnimSequence> RestLoop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Life|Rest")
	TObjectPtr<UAnimSequence> RestEnd = nullptr;


	// ========================================================================
	// Animation - Reaction / AI
	// ========================================================================

	/** notice01 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Reaction")
	TObjectPtr<UAnimSequence> Notice = nullptr;

	/** roar01 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Reaction")
	TObjectPtr<UAnimSequence> Roar = nullptr;

	/** glad01 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Reaction")
	TObjectPtr<UAnimSequence> Glad = nullptr;

	/** hate01 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Reaction")
	TObjectPtr<UAnimSequence> Hate = nullptr;

	/** refresh01 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Reaction")
	TObjectPtr<UAnimSequence> Refresh = nullptr;


	// ========================================================================
	// Animation - Step Out
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Action|StepOut")
	TObjectPtr<UAnimSequence> StepOutStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Action|StepOut")
	TObjectPtr<UAnimSequence> StepOut = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Action|StepOut")
	TObjectPtr<UAnimSequence> StepOutEnd = nullptr;


	// ========================================================================
	// Animation - Face
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Face")
	TObjectPtr<UAnimSequence> Eye = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Face")
	TObjectPtr<UAnimSequence> Mouth = nullptr;


	// ========================================================================
	// Animation - Extra
	// ========================================================================

	/**
	 * 특정 포켓몬만 가지고 있는 특수 애니메이션.
	 *
	 * 일반 공통 애니메이션은 위 슬롯에 넣고
	 * 예외적인 애니메이션만 여기에 넣습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Animation|Extra")
	TMap<FGameplayTag, TObjectPtr<UAnimSequence>> ExtraAnimations;


	// ========================================================================
	// Collision
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Collision")
	float CapsuleRadius = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Collision")
	float CapsuleHalfHeight = 88.0f;


	// ========================================================================
	// Movement
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Movement")
	float MoveSpeed = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Movement")
	float MaxStepHeight = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Movement")
	float WalkableFloorAngleDegrees = 44.0f;


	// ========================================================================
	// Stats
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Stats")
	float MaxHP = 100.0f;

	// GAS의 기본 공격력이다. 실제 기술 피해량은 GameplayEffect 계산식에서 이 값을 읽는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|GAS|Attribute", meta = (ClampMin = "0.0"))
	float BaseAttackPower = 10.0f;

	// GAS의 기본 방어력이다. 방어 공식은 기술별 GameplayEffect 또는 ExecutionCalculation에서 결정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|GAS|Attribute", meta = (ClampMin = "0.0"))
	float BaseDefense = 0.0f;

	// 포켓몬이 생성될 때 ASC에 자동으로 부여할 어빌리티 목록이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|GAS|Ability")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;


	// ========================================================================
	// Skills
	// ========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Skill")
	TArray<FUEPokemonSkillData> Skills;


	// ========================================================================
	// Functions
	// ========================================================================

	/**
	 * GameplayTag로 이 포켓몬의 애니메이션을 찾습니다.
	 *
	 * 예:
	 * Pokemon_Animation_Walk
	 *      -> 현재 포켓몬의 Walk 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	UAnimSequence* FindAnimationByTag(const FGameplayTag& AnimationTag) const;

	/**
	 * 해당 애니메이션이 등록되어 있는지 확인합니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
	bool HasAnimation(const FGameplayTag& AnimationTag) const;
};
