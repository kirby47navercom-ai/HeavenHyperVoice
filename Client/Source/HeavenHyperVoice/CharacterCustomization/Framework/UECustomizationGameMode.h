#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UECustomizationGameMode.generated.h"

/** GameMode used only by the independent character-customization level. */
UCLASS()
class HEAVENHYPERVOICE_API AUECustomizationGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AUECustomizationGameMode();
};
