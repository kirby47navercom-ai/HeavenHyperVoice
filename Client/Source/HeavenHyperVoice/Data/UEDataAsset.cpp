// Fill out your copyright notice in the Description page of Project Settings.


#include "UEDataAsset.h"

const UInputAction* UUEDataAsset::FindInputActionByTag(const FGameplayTag& InputTag) const
{
	for (const FR1InputAction& Action : InputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}

	
	return nullptr;
}
