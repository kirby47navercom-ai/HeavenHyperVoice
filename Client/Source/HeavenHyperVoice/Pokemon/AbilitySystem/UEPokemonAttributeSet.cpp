#include "UEPokemonAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UUEPokemonAttributeSet::UUEPokemonAttributeSet()
{
	// DataAsset이 아직 연결되지 않은 포켓몬도 안전하게 생성될 수 있는 기본값이다.
	InitMaxHealth(100.0f);
	InitHealth(100.0f);
	InitAttackPower(10.0f);
	InitDefense(0.0f);
}

void UUEPokemonAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// REPNOTIFY_Always를 사용해야 예측값과 서버 확정값이 같아도 GAS 델리게이트가 정리된다.
	DOREPLIFETIME_CONDITION_NOTIFY(UUEPokemonAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UUEPokemonAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UUEPokemonAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UUEPokemonAttributeSet, Defense, COND_None, REPNOTIFY_Always);
}

void UUEPokemonAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 잘못된 GameplayEffect가 들어와도 체력 범위와 전투 수치가 깨지지 않게 막는다.
	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetAttackPowerAttribute() || Attribute == GetDefenseAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UUEPokemonAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// GameplayEffect 계산이 끝난 뒤에도 최종 체력은 반드시 0과 최대 체력 사이에 둔다.
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(GetMaxHealth(), 1.0f));
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
}

void UUEPokemonAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UUEPokemonAttributeSet, Health, OldHealth);
}

void UUEPokemonAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UUEPokemonAttributeSet, MaxHealth, OldMaxHealth);
}

void UUEPokemonAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UUEPokemonAttributeSet, AttackPower, OldAttackPower);
}

void UUEPokemonAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldDefense)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UUEPokemonAttributeSet, Defense, OldDefense);
}
