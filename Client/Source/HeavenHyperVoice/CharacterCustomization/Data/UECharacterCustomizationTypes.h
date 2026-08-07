#pragma once

#include "CoreMinimal.h"
#include "UECharacterCustomizationTypes.generated.h"

/** Body presets change only the preview mesh proportions, never gameplay collision. */
UENUM(BlueprintType)
enum class EUEBodyPreset : uint8
{
	Slim,
	Athletic,
	Heavy
};

/**
 * Serializable appearance chosen in the customization scene.
 *
 * The values are asset-independent so the temporary preview mannequin can be
 * replaced by production modular skeletal meshes without changing save data.
 */
USTRUCT(BlueprintType)
struct HEAVENHYPERVOICE_API FUECharacterCustomizationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	EUEBodyPreset BodyPreset = EUEBodyPreset::Athletic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Height = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeadSize = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShoulderWidth = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization", meta = (ClampMin = "0", ClampMax = "3"))
	int32 HairStyle = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization", meta = (ClampMin = "0", ClampMax = "2"))
	int32 AccessoryStyle = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	FLinearColor SkinColor = FLinearColor(0.72f, 0.48f, 0.34f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	FLinearColor HairColor = FLinearColor(0.025f, 0.04f, 0.07f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	FLinearColor OutfitColor = FLinearColor(0.02f, 0.55f, 0.95f, 1.0f);

	void Normalize();
};
