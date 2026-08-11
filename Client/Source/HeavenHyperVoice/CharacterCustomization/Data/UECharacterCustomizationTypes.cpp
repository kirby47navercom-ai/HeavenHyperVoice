#include "UECharacterCustomizationTypes.h"

void FUECharacterCustomizationData::Normalize()
{
	Height = FMath::Clamp(Height, 0.0f, 1.0f);
	HeadSize = FMath::Clamp(HeadSize, 0.0f, 1.0f);
	ShoulderWidth = FMath::Clamp(ShoulderWidth, 0.0f, 1.0f);
	FaceStyle = FMath::Max(0, FaceStyle);
	EyeWhiteStyle = FMath::Max(0, EyeWhiteStyle);
	EyeIrisStyle = FMath::Max(0, EyeIrisStyle);
	EyeHighlightStyle = FMath::Max(0, EyeHighlightStyle);
	EyeExtraStyle = FMath::Max(0, EyeExtraStyle);
	BrowStyle = FMath::Max(0, BrowStyle);
	EyelashStyle = FMath::Max(0, EyelashStyle);
	EyelineStyle = FMath::Max(0, EyelineStyle);
	MouthStyle = FMath::Max(0, MouthStyle);
	LipStyle = FMath::Max(0, LipStyle);
	MouthLineStyle = FMath::Max(0, MouthLineStyle);
	HairFrontStyle = FMath::Max(0, HairFrontStyle);
	HairSideStyle = FMath::Max(0, HairSideStyle);
	HairBackStyle = FMath::Max(0, HairBackStyle);
	HairExtraStyle = FMath::Max(0, HairExtraStyle);
	HairBaseStyle = FMath::Max(0, HairBaseStyle);
	TopStyle = FMath::Max(0, TopStyle);
	BottomStyle = FMath::Max(0, BottomStyle);
	OnepieceStyle = FMath::Max(0, OnepieceStyle);
	ShoesStyle = FMath::Max(0, ShoesStyle);
	HeadAccessoryStyle = FMath::Max(0, HeadAccessoryStyle);
	FaceAccessoryStyle = FMath::Max(0, FaceAccessoryStyle);
	EarAccessoryStyle = FMath::Max(0, EarAccessoryStyle);
	TailAccessoryStyle = FMath::Max(0, TailAccessoryStyle);
	NeckAccessoryStyle = FMath::Max(0, NeckAccessoryStyle);
	if (OnepieceStyle > 0)
	{
		TopStyle = 0;
		BottomStyle = 0;
	}
	SkinColor.A = 1.0f;
	HairColor.A = 1.0f;
	EyeColor.A = 1.0f;
	LipColor.A = 1.0f;
	OutfitColor.A = 1.0f;
	TopColor.A = 1.0f;
	BottomColor.A = 1.0f;
	OnepieceColor.A = 1.0f;
	ShoesColor.A = 1.0f;
	AccessoryColor.A = 1.0f;
}
