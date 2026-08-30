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
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
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

	/**
	 * 전국도감 번호. 저장된 파트너가 카탈로그 배열 순서에 영향을 받지 않게 한다.
	 *
	 * 카탈로그 위치 대신 이 값을 사용하며, 에셋 파일명의 도감번호와 일치해야 한다.
	 * 0이면 유효한 종족으로 등록하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Identity", meta = (ClampMin = "0"))
	int32 DexNumber = 0;

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
	// Audio - Cry
	// ========================================================================

	/**
	 * 동행 포켓몬이 플레이어의 소환 요청으로 필드에 나타날 때 한 번 재생할 대표 울음소리다.
	 * SoundWave, SoundCue, MetaSound Source처럼 USoundBase를 상속한 에셋을 지정할 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry")
	TObjectPtr<USoundBase> SummonCry = nullptr;

	/**
	 * 소환할 때 무작위로 고를 울음소리 후보들이다.
	 * 여러 파일을 등록하면 R 소환마다 같은 소리만 반복되지 않으며, 배열이 비어 있으면 SummonCry를 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry|Summon")
	TArray<TObjectPtr<USoundBase>> SummonCries;

	/** 체력이 0이 되어 기절할 때 사용할 대표 소리다. FaintCries가 비어 있을 때 대체재로 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry")
	TObjectPtr<USoundBase> FaintCry = nullptr;

	/** 기절할 때 무작위로 고를 후보들이다. 비어 있으면 FaintCry를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry|Faint")
	TArray<TObjectPtr<USoundBase>> FaintCries;

	/**
	 * 야생 상태에서 무작위로 고를 울음소리 후보들이다.
	 * 배열이 비어 있으면 SummonCry를 대신 사용하므로 같은 소리를 두 번 지정할 필요가 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry")
	TArray<TObjectPtr<USoundBase>> WildCries;

	/** 평온하거나 기분이 좋을 때 사용할 울음소리 후보들이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry|Emotion")
	TArray<TObjectPtr<USoundBase>> HappyCries;

	/** 적을 발견했거나 전투에 들어갈 때 사용할 분노 울음소리 후보들이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry|Emotion")
	TArray<TObjectPtr<USoundBase>> AngryCries;

	/** 피해를 받았거나 슬픈 상태를 표현할 때 사용할 울음소리 후보들이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry|Emotion")
	TArray<TObjectPtr<USoundBase>> SadCries;

	/** 몸으로 직접 타격하는 기술이 시작될 때 사용할 짧은 공격 울음소리 후보들이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry|Combat")
	TArray<TObjectPtr<USoundBase>> PhysicalAttackCries;

	/** 원거리·속성 기술이 시작될 때 사용할 특수공격 울음소리 후보들이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry|Combat")
	TArray<TObjectPtr<USoundBase>> SpecialAttackCries;

	/** 종별 개성을 드러내는 특수음성 후보들이다. 소환이나 연출에서 선택적으로 재사용할 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry|Special")
	TArray<TObjectPtr<USoundBase>> SpecialCries;

	/** 필드에서 쉬거나 주변을 살필 때 사용할 환경 반응 울음소리 후보들이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry|Ambient")
	TArray<TObjectPtr<USoundBase>> AmbientCries;

	/** 야생 포켓몬의 랜덤 울음을 끄고 싶을 때 해제한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry")
	bool bEnableWildCries = true;

	/** 야생 울음 사이의 최소 대기 시간(초)이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry", meta = (ClampMin = "0.1", UIMin = "1.0"))
	float WildCryMinIntervalSeconds = 8.0f;

	/** 야생 울음 사이의 최대 대기 시간(초)이다. 최소값보다 작으면 런타임에서 최소값으로 맞춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry", meta = (ClampMin = "0.1", UIMin = "1.0"))
	float WildCryMaxIntervalSeconds = 20.0f;

	/** 이 포켓몬 울음소리의 최종 볼륨 배율이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float CryVolumeMultiplier = 1.0f;

	/** 이 포켓몬 울음소리의 최종 피치 배율이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Cry", meta = (ClampMin = "0.01", UIMin = "0.5", UIMax = "2.0"))
	float CryPitchMultiplier = 1.0f;

	/**
	 * 울음을 위치 기반 3D 음향으로 만드는 거리 감쇠 설정이다.
	 * 프로젝트 공용 Pokemon Cry 감쇠 에셋을 지정하되, 종별로 필요하면 다른 에셋으로 교체할 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Spatial")
	TObjectPtr<USoundAttenuation> CryAttenuation = nullptr;

	/** 가까운 야생 포켓몬이 동시에 울 때 과도하게 겹치는 것을 제한하는 공용 동시 재생 설정이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Spatial")
	TObjectPtr<USoundConcurrency> CryConcurrency = nullptr;


	// ========================================================================
	// 오디오 - 포켓몬 행동 효과음
	// ========================================================================

	// 걷기와 달리기 중 발이 지면에 닿을 때 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Movement")
	TArray<TObjectPtr<USoundBase>> FootstepSounds;

	// 점프가 시작되는 순간 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Movement")
	TArray<TObjectPtr<USoundBase>> JumpSounds;

	// 착지하는 순간 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Movement")
	TArray<TObjectPtr<USoundBase>> LandingSounds;

	// 수영 동작 중 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Movement")
	TArray<TObjectPtr<USoundBase>> SwimSounds;

	// 비행, 부유처럼 일반 발걸음으로 표현할 수 없는 이동에 사용하는 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Movement")
	TArray<TObjectPtr<USoundBase>> SpecialMovementSounds;

	// 공격 동작이 시작될 때 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Combat")
	TArray<TObjectPtr<USoundBase>> AttackSounds;

	// 공격에 맞았을 때 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Combat")
	TArray<TObjectPtr<USoundBase>> HitSounds;

	// 큰 충격으로 쓰러지는 동작에 사용하는 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Combat")
	TArray<TObjectPtr<USoundBase>> DownSounds;

	// 전투 불능 상태가 확정될 때 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Combat")
	TArray<TObjectPtr<USoundBase>> FaintEffectSounds;

	// 먹기 동작 중 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Life")
	TArray<TObjectPtr<USoundBase>> EatSounds;

	// 기절 상태에 진입하거나 유지될 때 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Status")
	TArray<TObjectPtr<USoundBase>> StunSounds;

	// 잠들거나 수면 상태를 유지할 때 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Status")
	TArray<TObjectPtr<USoundBase>> SleepSounds;

	// 필드에 등장하는 순간 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Lifecycle")
	TArray<TObjectPtr<USoundBase>> SpawnSounds;

	// 필드에서 사라지는 순간 재생할 후보 효과음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Audio|Effect|Lifecycle")
	TArray<TObjectPtr<USoundBase>> DespawnSounds;


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
