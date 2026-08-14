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

	for (const FUEPlayerSequenceEntry& Entry : SequenceEntries)
	{
		if (Entry.Sequence && Entry.SequenceTag == SequenceTag)
		{
			return Entry.Sequence;
		}
	}

	return nullptr;
}
