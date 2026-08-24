#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UEPokemonAnimationDataAsset.generated.h"

class UAnimSequence;


/**
 * 포켓몬 한 종류가 사용하는 애니메이션 모음.
 *
 * 예:
 * DA_PikachuAnimation
 * DA_EeveeAnimation
 * DA_BulbasaurAnimation
 *
 * 실제 게임 코드에서는 애니메이션 파일을 직접 참조하지 않고
 * GameplayTag를 이용하여 FindSequenceByTag()로 찾습니다.
 */
UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEPokemonAnimationDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:

    // ========================================================================
    // Idle
    // ========================================================================

    /** defaultwait01_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Idle")
    TObjectPtr<UAnimSequence> Idle = nullptr;

    /** defaultidle01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Idle")
    TObjectPtr<UAnimSequence> Idle01 = nullptr;

    /** defaultidle02 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Idle")
    TObjectPtr<UAnimSequence> Idle02 = nullptr;

    /** battlewait01_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Idle")
    TObjectPtr<UAnimSequence> BattleIdle = nullptr;


    // ========================================================================
    // Locomotion
    // ========================================================================

    /** walk01_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Locomotion")
    TObjectPtr<UAnimSequence> Walk = nullptr;

    /** run01_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Locomotion")
    TObjectPtr<UAnimSequence> Run = nullptr;

    /** turn_l090 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Locomotion")
    TObjectPtr<UAnimSequence> TurnLeft90 = nullptr;

    /** turn_r090 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Locomotion")
    TObjectPtr<UAnimSequence> TurnRight90 = nullptr;


    // ========================================================================
    // Jump / Fall
    // ========================================================================

    /** jumpup01_start */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Jump")
    TObjectPtr<UAnimSequence> JumpStart = nullptr;

    /** jumpup01_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Jump")
    TObjectPtr<UAnimSequence> JumpLoop = nullptr;

    /** jumpdown01_start */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Jump")
    TObjectPtr<UAnimSequence> FallStart = nullptr;

    /** jumpdown01_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Jump")
    TObjectPtr<UAnimSequence> FallLoop = nullptr;

    /** land02 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Jump")
    TObjectPtr<UAnimSequence> Land = nullptr;


    // ========================================================================
    // Combat - Physical Attack
    // ========================================================================

    /** attack01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Combat")
    TObjectPtr<UAnimSequence> Attack01 = nullptr;

    /** attack02 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Combat")
    TObjectPtr<UAnimSequence> Attack02 = nullptr;


    // ========================================================================
    // Combat - Range Attack
    // ========================================================================

    /** rangeattack01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Combat")
    TObjectPtr<UAnimSequence> RangeAttack01 = nullptr;

    /** rangeattack02_start */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Combat")
    TObjectPtr<UAnimSequence> RangeAttack02Start = nullptr;

    /** rangeattack02_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Combat")
    TObjectPtr<UAnimSequence> RangeAttack02Loop = nullptr;

    /** rangeattack02_end */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Combat")
    TObjectPtr<UAnimSequence> RangeAttack02End = nullptr;


    // ========================================================================
    // Damage
    // ========================================================================

    /** damage01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Damage")
    TObjectPtr<UAnimSequence> Damage01 = nullptr;

    /** damage02 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Damage")
    TObjectPtr<UAnimSequence> Damage02 = nullptr;


    // ========================================================================
    // Stun
    // ========================================================================

    /** stun01_start */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Status|Stun")
    TObjectPtr<UAnimSequence> StunStart = nullptr;

    /** stun01_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Status|Stun")
    TObjectPtr<UAnimSequence> StunLoop = nullptr;

    /** stun01_end */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Status|Stun")
    TObjectPtr<UAnimSequence> StunEnd = nullptr;


    // ========================================================================
    // Down
    // ========================================================================

    /** down01_start */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Status|Down")
    TObjectPtr<UAnimSequence> DownStart = nullptr;

    /** down01_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Status|Down")
    TObjectPtr<UAnimSequence> DownLoop = nullptr;

    /** down01_end */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Status|Down")
    TObjectPtr<UAnimSequence> DownEnd = nullptr;


    // ========================================================================
    // Eat 01
    // ========================================================================

    /** eat01_start */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Eat01")
    TObjectPtr<UAnimSequence> Eat01Start = nullptr;

    /** eat01_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Eat01")
    TObjectPtr<UAnimSequence> Eat01Loop = nullptr;

    /** eat01_end */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Eat01")
    TObjectPtr<UAnimSequence> Eat01End = nullptr;


    // ========================================================================
    // Eat 02
    // ========================================================================

    /** eat02_start */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Eat02")
    TObjectPtr<UAnimSequence> Eat02Start = nullptr;

    /** eat02_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Eat02")
    TObjectPtr<UAnimSequence> Eat02Loop = nullptr;

    /** eat02_end */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Eat02")
    TObjectPtr<UAnimSequence> Eat02End = nullptr;


    // ========================================================================
    // Sleep
    // ========================================================================

    /** sleep01_start */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Sleep")
    TObjectPtr<UAnimSequence> SleepStart = nullptr;

    /** sleep01_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Sleep")
    TObjectPtr<UAnimSequence> SleepLoop = nullptr;

    /** sleep01_end */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Sleep")
    TObjectPtr<UAnimSequence> SleepEnd = nullptr;


    // ========================================================================
    // Rest
    // ========================================================================

    /** rest01_start */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Rest")
    TObjectPtr<UAnimSequence> RestStart = nullptr;

    /** rest01_loop */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Rest")
    TObjectPtr<UAnimSequence> RestLoop = nullptr;

    /** rest01_end */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Life|Rest")
    TObjectPtr<UAnimSequence> RestEnd = nullptr;


    // ========================================================================
    // Reaction / AI
    // ========================================================================

    /** notice01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Reaction")
    TObjectPtr<UAnimSequence> Notice = nullptr;

    /** roar01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Reaction")
    TObjectPtr<UAnimSequence> Roar = nullptr;

    /** glad01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Reaction")
    TObjectPtr<UAnimSequence> Glad = nullptr;

    /** hate01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Reaction")
    TObjectPtr<UAnimSequence> Hate = nullptr;

    /** refresh01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Reaction")
    TObjectPtr<UAnimSequence> Refresh = nullptr;


    // ========================================================================
    // Step Out
    // ========================================================================

    /** stepout01_start */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Action|StepOut")
    TObjectPtr<UAnimSequence> StepOutStart = nullptr;

    /** stepout01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Action|StepOut")
    TObjectPtr<UAnimSequence> StepOut = nullptr;

    /** stepout01_end */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Action|StepOut")
    TObjectPtr<UAnimSequence> StepOutEnd = nullptr;


    // ========================================================================
    // Face
    // ========================================================================

    /** eye01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Face")
    TObjectPtr<UAnimSequence> Eye = nullptr;

    /** mouth01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Face")
    TObjectPtr<UAnimSequence> Mouth = nullptr;


    // ========================================================================
    // Extra Animations
    // ========================================================================

    /**
     * 위에 정의하지 않은 포켓몬별 특수 애니메이션.
     *
     * 예:
     * Pokemon.Animation.Special.Whatever
     *
     * 포켓몬별로만 존재하는 특수 애니메이션이 생겼을 때
     * C++ 클래스를 다시 수정하지 않고 여기에서 추가할 수 있습니다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon Animation|Extra")
    TMap<FGameplayTag, TObjectPtr<UAnimSequence>> ExtraAnimations;


public:

    // ========================================================================
    // Lookup
    // ========================================================================

    /**
     * GameplayTag에 해당하는 애니메이션을 찾습니다.
     *
     * 찾지 못하면 nullptr을 반환합니다.
     */
    UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
    UAnimSequence* FindSequenceByTag(FGameplayTag AnimationTag) const;


    /**
     * 해당 태그에 사용할 수 있는 애니메이션이 존재하는지 확인합니다.
     */
    UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
    bool HasSequenceByTag(FGameplayTag AnimationTag) const;


    /**
     * 현재 DataAsset에 등록되어 있는 애니메이션 개수를 반환합니다.
     */
    UFUNCTION(BlueprintPure, Category = "Pokemon|Animation")
    int32 GetConfiguredAnimationCount() const;
};
