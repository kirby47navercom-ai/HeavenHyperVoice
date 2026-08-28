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
	// 같은 어빌리티를 중복 등록하지 않고 포켓몬이 시작할 때 사용할 능력을 부여한다.
	UFUNCTION(BlueprintCallable, Category = "GAS|Ability")
	void AddCharacterAbilities(const TArray<TSubclassOf<class UGameplayAbility>>& StartupAbilities);

	// 종족이 바뀌었을 때 이전 능력을 지운 뒤 새 종족의 시작 능력으로 교체한다.
	UFUNCTION(BlueprintCallable, Category = "GAS|Ability")
	void SetCharacterAbilities(const TArray<TSubclassOf<class UGameplayAbility>>& StartupAbilities);

	// 전달받은 GameplayTag와 일치하는 어빌리티만 실행한다.
	UFUNCTION(BlueprintCallable, Category = "GAS|Ability")
	bool ActivateAbility(FGameplayTag AbilityTag);

	// 이 컴포넌트가 직접 부여한 어빌리티만 추적해 종족 교체 시 안전하게 제거한다.
	TArray<FGameplayAbilitySpecHandle> SpecHandles;
	
};
