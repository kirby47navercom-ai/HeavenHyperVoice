// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UEGameModeBase.generated.h"

/**
 * Base GameMode for the HeavenHyperVoice client.
 *
 * It owns the default player controller and player character classes used by
 * the login-first client flow.
 */
UCLASS()
class HEAVENHYPERVOICE_API AUEGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AUEGameModeBase();
};
