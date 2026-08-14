#include "UEPalworldCustomizationTypes.h"

#include "Engine/SkeletalMesh.h"

namespace
{
	const TArray<FUEPalworldCustomizationOption> EmptyOptions;
	const FUEPalworldCustomizationOption EmptyOption;

	USkeletalMesh* ResolvePalworldOutfitMesh(USkeletalMesh* Mesh)
	{
		if (!Mesh)
		{
			return nullptr;
		}

		const FString MeshPath = Mesh->GetPathName();
		if (!MeshPath.Contains(TEXT("/CharacterCustomization/Palworld/Assets/")) ||
			!MeshPath.Contains(TEXT("/Outfit/")) ||
			!MeshPath.Contains(TEXT("/SkeletalMeshes/")))
		{
			return Mesh;
		}

		FString CandidatePath = MeshPath;
		CandidatePath.ReplaceInline(TEXT("/Palworld/Assets/"), TEXT("/Palworld/AssetsFBX/"));
		CandidatePath.ReplaceInline(TEXT("/SkeletalMeshes/"), TEXT("/"));
		if (CandidatePath.Equals(MeshPath, ESearchCase::CaseSensitive))
		{
			return Mesh;
		}

		USkeletalMesh* CandidateMesh = LoadObject<USkeletalMesh>(nullptr, *CandidatePath);
		if (!CandidateMesh)
		{
			return Mesh;
		}

		// 일부 추출 의상은 /Assets 쪽 메쉬가 슬롯 1개짜리라 원본 머티리얼을 받을 수 없다.
		// 같은 이름의 /AssetsFBX 메쉬가 더 많은 슬롯을 가지고 있으면 그쪽을 사용한다.
		return CandidateMesh->GetMaterials().Num() >= Mesh->GetMaterials().Num() ? CandidateMesh : Mesh;
	}
}

USkeletalMesh* FUEPalworldCustomizationOption::LoadMesh(EUEPalworldGender Gender) const
{
	USkeletalMesh* Mesh = Gender == EUEPalworldGender::TypeA ? FemaleMesh.Get() : MaleMesh.Get();
	return ResolvePalworldOutfitMesh(Mesh);
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
