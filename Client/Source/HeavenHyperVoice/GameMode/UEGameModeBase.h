// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UEGameModeBase.generated.h"

/**
 * HeavenHyperVoice 클라이언트의 기본 GameMode다.
 *
 * 현재 단계에서는 서버 접속 전에 로그인 화면을 먼저 띄우는 PlayerController만 지정한다.
 */
UCLASS()
class HEAVENHYPERVOICE_API AUEGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AUEGameModeBase();
};
