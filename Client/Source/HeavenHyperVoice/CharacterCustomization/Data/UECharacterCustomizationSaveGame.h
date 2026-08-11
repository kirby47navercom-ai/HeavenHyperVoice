#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UECharacterCustomizationTypes.h"
#include "UECharacterCustomizationSaveGame.generated.h"

/** Save payload kept separate from login credentials and account validation. */
UCLASS()
class HEAVENHYPERVOICE_API UUECharacterCustomizationSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Customization")
	int32 DataVersion = 12;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Customization")
	FUECharacterCustomizationData Appearance;
};
