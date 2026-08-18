#include "UEPokemonAnimationDataAsset.h"

#include "../UEGameplayTags.h"

const UAnimSequence* UUEPokemonAnimationDataAsset::FindSequenceByTag(const FGameplayTag& SequenceTag) const
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

	for (const FUEPokemonSequenceEntry& Entry : SequenceEntries)
	{
		if (Entry.Sequence && Entry.SequenceTag == SequenceTag)
		{
			return Entry.Sequence;
		}
	}

	return nullptr;
}
