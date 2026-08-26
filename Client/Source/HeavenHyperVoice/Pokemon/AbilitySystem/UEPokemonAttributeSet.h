#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "UEPokemonAttributeSet.generated.h"

// GAS 속성마다 Getter, Setter, 초기화 함수와 FGameplayAttribute 접근 함수를 만든다.
#define UE_POKEMON_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 모든 포켓몬이 공통으로 사용하는 GAS 전투 수치다.
 * 종별 기본값은 UUEPokemonSpeciesData에서 읽고, 버프와 디버프는 GameplayEffect로 변경한다.
 */
UCLASS()
class HEAVENHYPERVOICE_API UUEPokemonAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UUEPokemonAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UE_POKEMON_ATTRIBUTE_ACCESSORS(UUEPokemonAttributeSet, Health)
	UE_POKEMON_ATTRIBUTE_ACCESSORS(UUEPokemonAttributeSet, MaxHealth)
	UE_POKEMON_ATTRIBUTE_ACCESSORS(UUEPokemonAttributeSet, AttackPower)
	UE_POKEMON_ATTRIBUTE_ACCESSORS(UUEPokemonAttributeSet, Defense)

protected:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Pokemon|GAS|Attribute")
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Pokemon|GAS|Attribute")
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Pokemon|GAS|Attribute")
	FGameplayAttributeData AttackPower;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Defense, Category = "Pokemon|GAS|Attribute")
	FGameplayAttributeData Defense;

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);

	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& OldDefense);
};

#undef UE_POKEMON_ATTRIBUTE_ACCESSORS
