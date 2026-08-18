#include "UEHHVCustomizationTypes.h"

#include "Engine/SkeletalMesh.h"

namespace
{
	const TArray<FUEHHVCustomizationOption> EmptyOptions;
	const FUEHHVCustomizationOption EmptyOption;
}

USkeletalMesh* FUEHHVCustomizationOption::LoadMesh(EUEHHVGender Gender) const
{
	return Gender == EUEHHVGender::TypeA ? FemaleMesh.Get() : MaleMesh.Get();
}

const TArray<FUEHHVCustomizationOption>& UUEHHVCustomizationCatalog::GetOptions(
	EUEHHVCustomizationCategory Category) const
{
	switch (Category)
	{
	case EUEHHVCustomizationCategory::Body:
		return BodyOptions;
	case EUEHHVCustomizationCategory::Head:
		return HeadOptions;
	case EUEHHVCustomizationCategory::Hair:
		return HairOptions;
	case EUEHHVCustomizationCategory::Eyes:
		return EyeOptions;
	case EUEHHVCustomizationCategory::BodyEquipment:
		return BodyEquipmentOptions;
	default:
		return EmptyOptions;
	}
}

int32 UUEHHVCustomizationCatalog::GetOptionCount(EUEHHVCustomizationCategory Category) const
{
	return GetOptions(Category).Num();
}

const FUEHHVCustomizationOption& UUEHHVCustomizationCatalog::GetOption(
	EUEHHVCustomizationCategory Category,
	int32 Index) const
{
	const TArray<FUEHHVCustomizationOption>& Options = GetOptions(Category);
	if (!Options.IsValidIndex(Index))
	{
		return EmptyOption;
	}
	return Options[Index];
}
