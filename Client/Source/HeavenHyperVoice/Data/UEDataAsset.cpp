// Fill out your copyright notice in the Description page of Project Settings.


#include "UEDataAsset.h"

#include "../UEGameplayTags.h"

const UInputAction* UUEDataAsset::FindInputActionByTag(const FGameplayTag& InputTag) const
{
	if (InputTag == UEGameplayTags::Input_Action_Move)
	{
		return MoveAction;
	}

	if (InputTag == UEGameplayTags::Input_Action_LookYaw)
	{
		return LookYawAction;
	}

	if (InputTag == UEGameplayTags::Input_Action_LookPitch)
	{
		return LookPitchAction;
	}

	if (InputTag == UEGameplayTags::Input_Action_Run)
	{
		return RunAction;
	}

	if (InputTag == UEGameplayTags::Input_Action_Roll)
	{
		return RollAction;
	}

	if (InputTag == UEGameplayTags::Input_Action_Jump)
	{
		return JumpAction;
	}

	if (InputTag == UEGameplayTags::Input_Action_SpawnPokemon)
	{
		return SpawnPokemonAction;
	}

	for (const FUEInputAction& Action : InputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}

	return nullptr;
}
