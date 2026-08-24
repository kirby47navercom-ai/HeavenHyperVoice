#include "UEPokemonSpeciesData.h"
#include "Animation/AnimSequence.h"
#include "UEPokemonGameplayTags.h"


UAnimSequence* UUEPokemonSpeciesData::FindAnimationByTag(
	const FGameplayTag& AnimationTag) const
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

	if (const TObjectPtr<UAnimSequence>* FoundAnimation =
		ExtraAnimations.Find(AnimationTag))
	{
		return FoundAnimation->Get();
	}


	return nullptr;
}


bool UUEPokemonSpeciesData::HasAnimation(
	const FGameplayTag& AnimationTag) const
{
	return FindAnimationByTag(AnimationTag) != nullptr;
}