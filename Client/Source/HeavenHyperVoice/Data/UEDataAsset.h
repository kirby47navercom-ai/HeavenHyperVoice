// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameplayTagContainer.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UEDataAsset.generated.h"

class UInputAction;
class UInputMappingContext;

USTRUCT(BlueprintType)
struct FUEInputAction
{
	GENERATED_BODY()

public:
	// Gameplay tag used by C++ to find the matching Enhanced Input Action.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag InputTag = FGameplayTag::EmptyTag;

	// Input Action asset bound to the tag above.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UInputAction> InputAction = nullptr;
};


UCLASS()
class HEAVENHYPERVOICE_API UUEDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	const UInputAction* FindInputActionByTag(const FGameplayTag& InputTag) const;

public:
	// Mapping context added to the local player when this character begins play.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UInputMappingContext> InputMappingContext;

	// Core character actions are explicit slots so Blueprint/DataAsset editing stays simple.
	// C++ still resolves them by GameplayTag, which keeps the character binding code scalable.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Move")
	TObjectPtr<UInputAction> MoveAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Look")
	TObjectPtr<UInputAction> LookYawAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Look")
	TObjectPtr<UInputAction> LookPitchAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
	TObjectPtr<UInputAction> RunAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
	TObjectPtr<UInputAction> RollAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
	TObjectPtr<UInputAction> JumpAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
	TObjectPtr<UInputAction> SpawnPokemonAction = nullptr;

	// 보유 포켓몬의 기술 슬롯 1~4에 대응하는 숫자키 공격 입력이다.
	// STT가 기술을 판정하는 단계가 추가돼도 같은 슬롯 체계를 재사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Pokemon")
	TObjectPtr<UInputAction> PokemonAttack1Action = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Pokemon")
	TObjectPtr<UInputAction> PokemonAttack2Action = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Pokemon")
	TObjectPtr<UInputAction> PokemonAttack3Action = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Pokemon")
	TObjectPtr<UInputAction> PokemonAttack4Action = nullptr;

	// Optional extension table for future actions that should also be found by GameplayTag.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FUEInputAction> InputActions;
};
