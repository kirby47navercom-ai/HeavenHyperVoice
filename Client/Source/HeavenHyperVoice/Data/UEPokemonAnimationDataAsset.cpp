#include "UEPokemonAnimationDataAsset.h"

#include "Animation/AnimSequence.h"
#include "../Pokemon/UEPokemonGameplayTags.h"


UAnimSequence* UUEPokemonAnimationDataAsset::FindSequenceByTag(
    FGameplayTag AnimationTag) const
{
    if (!AnimationTag.IsValid())
    {
        return nullptr;
    }


    // ========================================================================
    // Idle
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Idle.GetTag())
    {
        return Idle;
    }

    if (AnimationTag == Pokemon_Animation_Idle_01.GetTag())
    {
        return Idle01;
    }

    if (AnimationTag == Pokemon_Animation_Idle_02.GetTag())
    {
        return Idle02;
    }

    if (AnimationTag == Pokemon_Animation_BattleIdle.GetTag())
    {
        return BattleIdle;
    }


    // ========================================================================
    // Locomotion
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Walk.GetTag())
    {
        return Walk;
    }

    if (AnimationTag == Pokemon_Animation_Run.GetTag())
    {
        return Run;
    }

    if (AnimationTag == Pokemon_Animation_Turn_Left90.GetTag())
    {
        return TurnLeft90;
    }

    if (AnimationTag == Pokemon_Animation_Turn_Right90.GetTag())
    {
        return TurnRight90;
    }


    // ========================================================================
    // Jump / Fall
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Jump_Start.GetTag())
    {
        return JumpStart;
    }

    if (AnimationTag == Pokemon_Animation_Jump_Loop.GetTag())
    {
        return JumpLoop;
    }

    if (AnimationTag == Pokemon_Animation_Fall_Start.GetTag())
    {
        return FallStart;
    }

    if (AnimationTag == Pokemon_Animation_Fall_Loop.GetTag())
    {
        return FallLoop;
    }

    if (AnimationTag == Pokemon_Animation_Land.GetTag())
    {
        return Land;
    }


    // ========================================================================
    // Combat
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Attack_01.GetTag())
    {
        return Attack01;
    }

    if (AnimationTag == Pokemon_Animation_Attack_02.GetTag())
    {
        return Attack02;
    }

    if (AnimationTag == Pokemon_Animation_RangeAttack_01.GetTag())
    {
        return RangeAttack01;
    }

    if (AnimationTag == Pokemon_Animation_RangeAttack_02_Start.GetTag())
    {
        return RangeAttack02Start;
    }

    if (AnimationTag == Pokemon_Animation_RangeAttack_02_Loop.GetTag())
    {
        return RangeAttack02Loop;
    }

    if (AnimationTag == Pokemon_Animation_RangeAttack_02_End.GetTag())
    {
        return RangeAttack02End;
    }


    // ========================================================================
    // Damage
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Damage_01.GetTag())
    {
        return Damage01;
    }

    if (AnimationTag == Pokemon_Animation_Damage_02.GetTag())
    {
        return Damage02;
    }


    // ========================================================================
    // Stun
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Stun_Start.GetTag())
    {
        return StunStart;
    }

    if (AnimationTag == Pokemon_Animation_Stun_Loop.GetTag())
    {
        return StunLoop;
    }

    if (AnimationTag == Pokemon_Animation_Stun_End.GetTag())
    {
        return StunEnd;
    }


    // ========================================================================
    // Down
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Down_Start.GetTag())
    {
        return DownStart;
    }

    if (AnimationTag == Pokemon_Animation_Down_Loop.GetTag())
    {
        return DownLoop;
    }

    if (AnimationTag == Pokemon_Animation_Down_End.GetTag())
    {
        return DownEnd;
    }


    // ========================================================================
    // Eat 01
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Eat_01_Start.GetTag())
    {
        return Eat01Start;
    }

    if (AnimationTag == Pokemon_Animation_Eat_01_Loop.GetTag())
    {
        return Eat01Loop;
    }

    if (AnimationTag == Pokemon_Animation_Eat_01_End.GetTag())
    {
        return Eat01End;
    }


    // ========================================================================
    // Eat 02
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Eat_02_Start.GetTag())
    {
        return Eat02Start;
    }

    if (AnimationTag == Pokemon_Animation_Eat_02_Loop.GetTag())
    {
        return Eat02Loop;
    }

    if (AnimationTag == Pokemon_Animation_Eat_02_End.GetTag())
    {
        return Eat02End;
    }


    // ========================================================================
    // Sleep
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Sleep_Start.GetTag())
    {
        return SleepStart;
    }

    if (AnimationTag == Pokemon_Animation_Sleep_Loop.GetTag())
    {
        return SleepLoop;
    }

    if (AnimationTag == Pokemon_Animation_Sleep_End.GetTag())
    {
        return SleepEnd;
    }


    // ========================================================================
    // Rest
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Rest_Start.GetTag())
    {
        return RestStart;
    }

    if (AnimationTag == Pokemon_Animation_Rest_Loop.GetTag())
    {
        return RestLoop;
    }

    if (AnimationTag == Pokemon_Animation_Rest_End.GetTag())
    {
        return RestEnd;
    }


    // ========================================================================
    // Reaction
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Notice.GetTag())
    {
        return Notice;
    }

    if (AnimationTag == Pokemon_Animation_Roar.GetTag())
    {
        return Roar;
    }

    if (AnimationTag == Pokemon_Animation_Glad.GetTag())
    {
        return Glad;
    }

    if (AnimationTag == Pokemon_Animation_Hate.GetTag())
    {
        return Hate;
    }

    if (AnimationTag == Pokemon_Animation_Refresh.GetTag())
    {
        return Refresh;
    }


    // ========================================================================
    // Step Out
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_StepOut_Start.GetTag())
    {
        return StepOutStart;
    }

    if (AnimationTag == Pokemon_Animation_StepOut.GetTag())
    {
        return StepOut;
    }

    if (AnimationTag == Pokemon_Animation_StepOut_End.GetTag())
    {
        return StepOutEnd;
    }


    // ========================================================================
    // Face
    // ========================================================================

    if (AnimationTag == Pokemon_Animation_Eye.GetTag())
    {
        return Eye;
    }

    if (AnimationTag == Pokemon_Animation_Mouth.GetTag())
    {
        return Mouth;
    }


    // ========================================================================
    // Extra
    // ========================================================================

    if (const TObjectPtr<UAnimSequence>* FoundSequence =
        ExtraAnimations.Find(AnimationTag))
    {
        return FoundSequence->Get();
    }


    return nullptr;
}


bool UUEPokemonAnimationDataAsset::HasSequenceByTag(
    FGameplayTag AnimationTag) const
{
    return FindSequenceByTag(AnimationTag) != nullptr;
}


int32 UUEPokemonAnimationDataAsset::GetConfiguredAnimationCount() const
{
    int32 Count = 0;

    const auto CountIfValid = [&Count](const TObjectPtr<UAnimSequence>& Sequence)
    {
        if (Sequence)
        {
            ++Count;
        }
    };


    // Idle
    CountIfValid(Idle);
    CountIfValid(Idle01);
    CountIfValid(Idle02);
    CountIfValid(BattleIdle);

    // Locomotion
    CountIfValid(Walk);
    CountIfValid(Run);
    CountIfValid(TurnLeft90);
    CountIfValid(TurnRight90);

    // Jump
    CountIfValid(JumpStart);
    CountIfValid(JumpLoop);
    CountIfValid(FallStart);
    CountIfValid(FallLoop);
    CountIfValid(Land);

    // Combat
    CountIfValid(Attack01);
    CountIfValid(Attack02);
    CountIfValid(RangeAttack01);
    CountIfValid(RangeAttack02Start);
    CountIfValid(RangeAttack02Loop);
    CountIfValid(RangeAttack02End);

    // Damage
    CountIfValid(Damage01);
    CountIfValid(Damage02);

    // Stun
    CountIfValid(StunStart);
    CountIfValid(StunLoop);
    CountIfValid(StunEnd);

    // Down
    CountIfValid(DownStart);
    CountIfValid(DownLoop);
    CountIfValid(DownEnd);

    // Eat 01
    CountIfValid(Eat01Start);
    CountIfValid(Eat01Loop);
    CountIfValid(Eat01End);

    // Eat 02
    CountIfValid(Eat02Start);
    CountIfValid(Eat02Loop);
    CountIfValid(Eat02End);

    // Sleep
    CountIfValid(SleepStart);
    CountIfValid(SleepLoop);
    CountIfValid(SleepEnd);

    // Rest
    CountIfValid(RestStart);
    CountIfValid(RestLoop);
    CountIfValid(RestEnd);

    // Reaction
    CountIfValid(Notice);
    CountIfValid(Roar);
    CountIfValid(Glad);
    CountIfValid(Hate);
    CountIfValid(Refresh);

    // StepOut
    CountIfValid(StepOutStart);
    CountIfValid(StepOut);
    CountIfValid(StepOutEnd);

    // Face
    CountIfValid(Eye);
    CountIfValid(Mouth);


    // Extra
    for (const TPair<FGameplayTag, TObjectPtr<UAnimSequence>>& Pair : ExtraAnimations)
    {
        if (Pair.Value)
        {
            ++Count;
        }
    }


    return Count;
}
