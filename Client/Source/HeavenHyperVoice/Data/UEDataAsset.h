// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameplayTagContainer.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UEDataAsset.generated.h"

class UInputAction;
class UInputMappingContext;

USTRUCT()
struct FR1InputAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag InputTag = FGameplayTag::EmptyTag;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UInputAction> InputAction = nullptr;
};


UCLASS()
class HEAVENHYPERVOICE_API UUEDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	const UInputAction* FindInputActionByTag(const FGameplayTag& InputTag) const;

public:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditDefaultsOnly)
	TArray<FR1InputAction> InputActions;

	/*UPROPERTY(EditDefaultsOnly)
	TArray<TObjectPtr<UInputAction>> TestInputActions;*/
	
};
