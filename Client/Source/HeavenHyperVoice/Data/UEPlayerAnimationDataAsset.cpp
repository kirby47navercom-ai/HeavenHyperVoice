#include "UEPlayerAnimationDataAsset.h"

#include "../UEGameplayTags.h"

const UAnimMontage* UUEPlayerAnimationDataAsset::FindMontageByTag(const FGameplayTag& MontageTag) const
{
	if (MontageTag == UEGameplayTags::State_Character_Roll && RollMontage)
	{
		return RollMontage;
	}
	if (MontageTag == UEGameplayTags::State_Character_Throw && ThrowMontage)
	{
		return ThrowMontage;
	}
	if (MontageTag == UEGameplayTags::State_Character_Damage && HitMontage)
	{
		return HitMontage;
	}
	if (MontageTag == UEGameplayTags::State_Character_Death && DeathMontage)
	{
		return DeathMontage;
	}

	for (const FUEPlayerMontageEntry& Entry : MontageEntries)
	{
		if (Entry.Montage && Entry.MontageTag == MontageTag)
		{
			return Entry.Montage;
		}
	}

	return nullptr;
}

const UAnimSequence* UUEPlayerAnimationDataAsset::FindSequenceByTag(const FGameplayTag& SequenceTag) const
{
	if (SequenceTag == UEGameplayTags::State_Character_Idle && IdleSequence)
	{
		return IdleSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Walk && WalkSequence)
	{
		return WalkSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Run && RunSequence)
	{
		return RunSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Jump && JumpSequence)
	{
		return JumpSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Fall && FallSequence)
	{
		return FallSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Roll && RollSequence)
	{
		return RollSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Landing && LandingSequence)
	{
		return LandingSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Holding && HoldingSequence)
	{
		return HoldingSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Throw && ThrowSequence)
	{
		return ThrowSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Damage && DamageSequence)
	{
		return DamageSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Death && DeathSequence)
	{
		return DeathSequence;
	}

	for (const FUEPlayerSequenceEntry& Entry : SequenceEntries)
	{
		if (Entry.Sequence && Entry.SequenceTag == SequenceTag)
		{
			return Entry.Sequence;
		}
	}

	return nullptr;
}

const UAnimSequence* UUEPlayerAnimationDataAsset::FindSequenceByTagForGender(
	const FGameplayTag& SequenceTag,
	EUEHHVGender Gender) const
{
	const FUEPlayerGenderAnimationSet& GenderSet = GetAnimationSetForGender(Gender);

	if (SequenceTag == UEGameplayTags::State_Character_Idle && GenderSet.IdleSequence)
	{
		return GenderSet.IdleSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Walk && GenderSet.WalkSequence)
	{
		return GenderSet.WalkSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Run && GenderSet.RunSequence)
	{
		return GenderSet.RunSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Jump && GenderSet.JumpSequence)
	{
		return GenderSet.JumpSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Fall && GenderSet.FallSequence)
	{
		return GenderSet.FallSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Roll && GenderSet.RollSequence)
	{
		return GenderSet.RollSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Landing && GenderSet.LandingSequence)
	{
		return GenderSet.LandingSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Holding && GenderSet.HoldingSequence)
	{
		return GenderSet.HoldingSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Throw && GenderSet.ThrowSequence)
	{
		return GenderSet.ThrowSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Damage && GenderSet.DamageSequence)
	{
		return GenderSet.DamageSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Death && GenderSet.DeathSequence)
	{
		return GenderSet.DeathSequence;
	}

	return FindSequenceByTag(SequenceTag);
}

const FUEPlayerGenderAnimationSet& UUEPlayerAnimationDataAsset::GetAnimationSetForGender(
	EUEHHVGender Gender) const
{
	return Gender == EUEHHVGender::TypeB ? TypeBAnimations : TypeAAnimations;
}

const UAnimSequence* UUEPlayerAnimationDataAsset::FindWalkSequenceForGender(EUEHHVGender Gender) const
{
	const FUEPlayerGenderAnimationSet& GenderSet = GetAnimationSetForGender(Gender);
	return GenderSet.WalkSequence ? GenderSet.WalkSequence : WalkSequence;
}
