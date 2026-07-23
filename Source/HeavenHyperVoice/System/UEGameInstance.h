// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "UEGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENHYPERVOICE_API UUEGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	
	UUEGameInstance(const FObjectInitializer& ObjectInitializer);

public:
	virtual void Init() override;
	virtual void Shutdown() override;
	
};
