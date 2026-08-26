#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UEPokemonSpeciesData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;
class UAnimSequence;


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

	/**
	 * 도감번호. 서버가 파트너를 지목할 때 쓰는 값이다 (파이리 4, 피카츄 25).
	 *
	 * 카탈로그 배열 위치로 종족을 찾던 것을 이 번호로 바꿨다. 배열 순서가 밀리면
	 * 이미 저장된 파트너가 조용히 다른 종족이 되기 때문이다. 이 번호는 에셋
	 * 파일명(pm0004_...)에 이미 박혀 있고 배열 순서와 무관하다.
	 *
	 * 0 이면 서버가 이 종족을 지목할 수 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Identity", meta = (ClampMin = "0"))
	int32 DexNumber = 0;


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