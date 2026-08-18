#include "UEPlayerAnimationDataAsset.h"

#include "../UEGameplayTags.h"

const UAnimMontage* UUEPlayerAnimationDataAsset::FindMontageByTag(const FGameplayTag& MontageTag) const
{
	if (MontageTag == UEGameplayTags::State_Character_Roll)
	{
		return RollMontage;
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
	if (SequenceTag == UEGameplayTags::State_Character_Idle)
	{
		return IdleSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Walk)
	{
		return WalkSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Run)
	{
		return RunSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Jump)
	{
		return JumpSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Fall)
	{
		return FallSequence;
	}

	if (SequenceTag == UEGameplayTags::State_Character_Roll)
	{
		return RollSequence;
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
