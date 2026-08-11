#pragma once

#include "CoreMinimal.h"
#include "UECharacterCustomizationTypes.generated.h"

UENUM(BlueprintType)
enum class EUECharacterGender : uint8
{
	Male,
	Female
};

/** Body presets deform every modular component together, never gameplay collision. */
UENUM(BlueprintType)
enum class EUEBodyPreset : uint8
{
	Slim,
	Athletic,
	Heavy
};

UENUM(BlueprintType)
enum class EUECustomizationPart : uint8
{
	Gender,
	Body,
	HairSet,
	FaceSkin,
	EyeWhite,
	EyeIris,
	EyeHighlight,
	EyeExtra,
	Brow,
	Eyelash,
	Eyeline,
	Mouth,
	Lip,
	MouthLine,
	HairFront,
	HairSide,
	HairBack,
	HairExtra,
	HairBase,
	Top,
	Bottom,
	Onepiece,
	Shoes,
	HeadAccessory,
	FaceAccessory,
	EarAccessory,
	TailAccessory,
	NeckAccessory
};

/**
 * Serializable appearance chosen in the customization scene.
 *
	 * Every index addresses one modular VRoid skeletal-mesh catalog. All selected
	 * modules use the same skeleton and are driven by the body leader pose.
 */
USTRUCT(BlueprintType)
struct HEAVENHYPERVOICE_API FUECharacterCustomizationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	EUECharacterGender Gender = EUECharacterGender::Male;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	EUEBodyPreset BodyPreset = EUEBodyPreset::Athletic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Height = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeadSize = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShoulderWidth = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 FaceStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 EyeWhiteStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 EyeIrisStyle = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 EyeHighlightStyle = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 EyeExtraStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 BrowStyle = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 EyelashStyle = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 EyelineStyle = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 MouthStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 LipStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 MouthLineStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 HairFrontStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 HairSideStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 HairBackStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 HairExtraStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 HairBaseStyle = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 TopStyle = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 BottomStyle = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 OnepieceStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 ShoesStyle = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 HeadAccessoryStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 FaceAccessoryStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 EarAccessoryStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 TailAccessoryStyle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Parts") int32 NeckAccessoryStyle = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	FLinearColor SkinColor = FLinearColor(0.863157f, 0.485150f, 0.309469f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	FLinearColor HairColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	FLinearColor EyeColor = FLinearColor(0.38f, 0.16f, 0.06f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	FLinearColor LipColor = FLinearColor::FromSRGBColor(FColor(196, 102, 116));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	FLinearColor OutfitColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Outfit")
	FLinearColor TopColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Outfit")
	FLinearColor BottomColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Outfit")
	FLinearColor OnepieceColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Outfit")
	FLinearColor ShoesColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Outfit")
	FLinearColor AccessoryColor = FLinearColor::White;

	void Normalize();
};
