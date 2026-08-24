#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UECharacterSelectionGameMode.generated.h"

/** 캐릭터 선택 UI만 실행하는 전용 게임 모드다. */
UCLASS()
class HEAVENHYPERVOICE_API AUECharacterSelectionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AUECharacterSelectionGameMode();
};
