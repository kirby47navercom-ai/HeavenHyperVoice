// Fill out your copyright notice in the Description page of Project Settings.


#include "UEAbilitySystemComponent.h"

void UUEAbilitySystemComponent::AddCharacterAbilities(
	const TArray<TSubclassOf<class UGameplayAbility>>& StartupAbilities)
{
	for (auto& AbilityClass : StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		FGameplayAbilitySpecHandle SpecHandle = GiveAbility(AbilitySpec);

		auto& a = ActivatableAbilities;

		//TryActivateAbility(SpecHandle);
		//GiveAbilityAndActivateOnce(AbilitySpec);
		SpecHandles.Add(SpecHandle);
	}
}

void UUEAbilitySystemComponent::ActivateAbility(FGameplayTag AbilityTag)
{
	for (FGameplayAbilitySpecHandle& SpecHandle : SpecHandles)
	{
		// TODO
		TryActivateAbility(SpecHandle);
	}
}
