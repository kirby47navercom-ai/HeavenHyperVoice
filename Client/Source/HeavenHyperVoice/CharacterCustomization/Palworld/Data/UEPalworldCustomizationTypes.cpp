#include "UEPalworldCustomizationTypes.h"

#include "Engine/SkeletalMesh.h"

namespace
{
	const TArray<FUEPalworldCustomizationOption> EmptyOptions;
	const FUEPalworldCustomizationOption EmptyOption;
}

USkeletalMesh* FUEPalworldCustomizationOption::LoadMesh(EUEPalworldGender Gender) const
{
	return Gender == EUEPalworldGender::TypeA ? FemaleMesh.Get() : MaleMesh.Get();
}

const TArray<FUEPalworldCustomizationOption>& UUEPalworldCustomizationCatalog::GetOptions(
	EUEPalworldCustomizationCategory Category) const
{
	switch (Category)
	{
	case EUEPalworldCustomizationCategory::Body:
		return BodyOptions;
	case EUEPalworldCustomizationCategory::Head:
		return HeadOptions;
	case EUEPalworldCustomizationCategory::Hair:
		return HairOptions;
	case EUEPalworldCustomizationCategory::Eyes:
		return EyeOptions;
	case EUEPalworldCustomizationCategory::BodyEquipment:
		return BodyEquipmentOptions;
	default:
		return EmptyOptions;
	}
}

int32 UUEPalworldCustomizationCatalog::GetOptionCount(EUEPalworldCustomizationCategory Category) const
{
	return GetOptions(Category).Num();
}

const FUEPalworldCustomizationOption& UUEPalworldCustomizationCatalog::GetOption(
	EUEPalworldCustomizationCategory Category,
	int32 Index) const
{
	const TArray<FUEPalworldCustomizationOption>& Options = GetOptions(Category);
	if (!Options.IsValidIndex(Index))
	{
		return EmptyOption;
	}
	return Options[Index];
}
