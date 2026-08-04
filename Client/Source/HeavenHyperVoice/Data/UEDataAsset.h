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
	TObjectPtr<UInputAction> MoveForwardAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Move")
	TObjectPtr<UInputAction> MoveBackwardAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Move")
	TObjectPtr<UInputAction> MoveRightAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Move")
	TObjectPtr<UInputAction> MoveLeftAction = nullptr;

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

	// Optional extension table for future actions that should also be found by GameplayTag.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FUEInputAction> InputActions;
};
