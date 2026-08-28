// Fill out your copyright notice in the Description page of Project Settings.


#include "UEAbilitySystemComponent.h"

void UUEAbilitySystemComponent::AddCharacterAbilities(
	const TArray<TSubclassOf<class UGameplayAbility>>& StartupAbilities)
{
	// 어빌리티 부여는 권한을 가진 쪽에서만 해야 멀티 환경에서 중복 Spec이 생기지 않는다.
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}

		// 같은 클래스가 이미 있다면 새 Spec을 만들지 않고 기존 핸들만 추적한다.
		if (FGameplayAbilitySpec* ExistingSpec = FindAbilitySpecFromClass(AbilityClass))
		{
			SpecHandles.AddUnique(ExistingSpec->Handle);
			continue;
		}

		const FGameplayAbilitySpec AbilitySpec(AbilityClass, 1);
		const FGameplayAbilitySpecHandle SpecHandle = GiveAbility(AbilitySpec);
		SpecHandles.Add(SpecHandle);
	}
}

void UUEAbilitySystemComponent::SetCharacterAbilities(
	const TArray<TSubclassOf<class UGameplayAbility>>& StartupAbilities)
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	// 다른 시스템이 부여한 어빌리티는 건드리지 않고 이 컴포넌트가 추적한 것만 제거한다.
	for (const FGameplayAbilitySpecHandle& SpecHandle : SpecHandles)
	{
		ClearAbility(SpecHandle);
	}
	SpecHandles.Reset();

	AddCharacterAbilities(StartupAbilities);
}

bool UUEAbilitySystemComponent::ActivateAbility(FGameplayTag AbilityTag)
{
	if (!AbilityTag.IsValid())
	{
		return false;
	}

	// 기존 코드는 태그를 무시하고 모든 Spec을 실행했다. 컨테이너 검색을 사용해 정확한 능력만 실행한다.
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(AbilityTag);
	return TryActivateAbilitiesByTag(AbilityTags);
}
