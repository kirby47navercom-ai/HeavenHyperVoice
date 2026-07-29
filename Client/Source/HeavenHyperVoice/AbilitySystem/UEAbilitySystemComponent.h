// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "UEAbilitySystemComponent.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENHYPERVOICE_API UUEAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	void AddCharacterAbilities(const TArray<TSubclassOf<class UGameplayAbility>>& StartupAbilities);
	
	void ActivateAbility(FGameplayTag AbilityTag);

	TArray<FGameplayAbilitySpecHandle> SpecHandles;
	
};
