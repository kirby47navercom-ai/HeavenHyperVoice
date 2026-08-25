#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UEFrontendGameMode.generated.h"

/** 프런트엔드 맵 설정을 Blueprint에서 보관하기 위한 최소 게임 모드 기반이다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEFrontendGameMode : public AGameModeBase
{
	GENERATED_BODY()
};
