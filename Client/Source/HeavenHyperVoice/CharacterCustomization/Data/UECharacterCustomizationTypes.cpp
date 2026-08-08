#include "UECharacterCustomizationTypes.h"

void FUECharacterCustomizationData::Normalize()
{
	Height = FMath::Clamp(Height, 0.0f, 1.0f);
	HeadSize = FMath::Clamp(HeadSize, 0.0f, 1.0f);
	ShoulderWidth = FMath::Clamp(ShoulderWidth, 0.0f, 1.0f);
	HairStyle = FMath::Clamp(HairStyle, 0, 3);
	AccessoryStyle = FMath::Clamp(AccessoryStyle, 0, 2);
	SkinColor.A = 1.0f;
	HairColor.A = 1.0f;
	OutfitColor.A = 1.0f;
}
