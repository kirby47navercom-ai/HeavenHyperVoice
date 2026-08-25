#pragma once

#include "UEHHVCustomizationPreviewActor.h"

#include "../Data/UEHHVCustomizationTypes.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Scene.h"
#include "Engine/Texture.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"

namespace UEHHVCustomizationPreviewActorPrivate
{
	inline const FUEHHVCustomizationOption PreviewEmptyOption;
	constexpr int32 PreviewMaxVisibleOutfits = 14;
	constexpr int32 PreviewFirstVisibleOutfitIndex = 1;

	inline USkeletalMeshComponent* CreateSkeletalPart(AActor* Owner, USceneComponent* Parent, const FName& Name)
	{
		USkeletalMeshComponent* Component = Owner->CreateDefaultSubobject<USkeletalMeshComponent>(Name);
		Component->SetupAttachment(Parent);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->bReceivesDecals = false;
		Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		// 커마 프리뷰에서는 선택할 때마다 idle 애니메이션이 위아래로 흔들리지 않게 정지 포즈로 둔다.
		Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Component->bPauseAnims = true;
		return Component;
	}

	inline void ApplyPreviewSignedMorphTarget(USkeletalMeshComponent* Component, const FName MinTarget, const FName MaxTarget, float Value)
	{
		if (!Component)
		{
			return;
		}

		const float ClampedValue = FMath::Clamp(Value, -1.0f, 1.0f);
		Component->SetMorphTarget(MinTarget, ClampedValue < 0.0f ? -ClampedValue : 0.0f);
		Component->SetMorphTarget(MaxTarget, ClampedValue > 0.0f ? ClampedValue : 0.0f);
	}

	inline void SetPreviewMaterialShownOnAllLods(USkeletalMeshComponent* Component, int32 MaterialIndex, bool bShow)
	{
		if (!Component || MaterialIndex < 0)
		{
			return;
		}

		USkeletalMesh* Mesh = Component->GetSkeletalMeshAsset();
		const FSkeletalMeshRenderData* RenderData = Mesh ? Mesh->GetResourceForRendering() : nullptr;
		if (!RenderData || RenderData->LODRenderData.IsEmpty())
		{
			Component->ShowMaterialSection(MaterialIndex, MaterialIndex, bShow, 0);
			return;
		}

		for (int32 LODIndex = 0; LODIndex < RenderData->LODRenderData.Num(); ++LODIndex)
		{
			const FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];
			bool bTouched = false;
			for (int32 SectionIndex = 0; SectionIndex < LODData.RenderSections.Num(); ++SectionIndex)
			{
				if (LODData.RenderSections[SectionIndex].MaterialIndex == MaterialIndex)
				{
					Component->ShowMaterialSection(MaterialIndex, SectionIndex, bShow, LODIndex);
					bTouched = true;
				}
			}

			if (!bTouched)
			{
				Component->ShowMaterialSection(MaterialIndex, MaterialIndex, bShow, LODIndex);
			}
		}
	}

	inline FString MakeMorphSafeMaterialName(const UMaterialInterface* Material)
	{
		FString AssetName = Material ? Material->GetName() : FString();
		for (int32 Index = 0; Index < AssetName.Len(); ++Index)
		{
			TCHAR& Character = AssetName[Index];
			if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
			{
				Character = TCHAR('_');
			}
		}
		return FString::Printf(TEXT("MI_MS_%s"), *AssetName);
	}

	inline void ApplyPreviewEyeColorParameters(UMaterialInstanceDynamic* Material, const FLinearColor& EyeColor)
	{
		if (!Material)
		{
			return;
		}

		const FLinearColor Color = EyeColor.GetClamped();
		for (const FName ParameterName : {TEXT("TintColor"), TEXT("Color"), TEXT("BaseColor"), TEXT("Base Color"), TEXT("IrisColor"), TEXT("Iris Color"), TEXT("EyeColor"), TEXT("Eye Color"), TEXT("MainColor")})
		{
			Material->SetVectorParameterValue(ParameterName, Color);
		}
	}

	inline void ApplyPreviewEyeTextureParameters(UMaterialInstanceDynamic* Material, UTexture* Texture)
	{
		if (!Material || !Texture)
		{
			return;
		}

		for (const FName ParameterName : {TEXT("Base Texture"), TEXT("BaseTexture"), TEXT("BaseMap"), TEXT("MainTex"), TEXT("Texture"), TEXT("Diffuse"), TEXT("Albedo")})
		{
			Material->SetTextureParameterValue(ParameterName, Texture);
		}
	}

	inline void EnsurePreviewSkeletalMaterialUsage(UMaterialInterface* Material)
	{
		if (!Material)
		{
			return;
		}

		// 추출 머티리얼 중 SkeletalMesh/MorphTargets 사용 플래그가 빠진 것이 있어,
		// 체형 모프 적용 뒤 회색 기본 머티리얼로 떨어지지 않게 적용 전에 확인한다.
		Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh);
		Material->CheckMaterialUsage_Concurrent(MATUSAGE_MorphTargets);
	}

	inline UMaterialInterface* LoadMeshLocalMaterial(
		const USkeletalMesh* Mesh,
		const UMaterialInterface* CurrentMaterial,
		int32 MaterialIndex,
		int32 MaterialCount)
	{
		if (!Mesh)
		{
			return nullptr;
		}

		FString MeshObjectPath = Mesh->GetPathName();
		int32 DotIndex = INDEX_NONE;
		if (MeshObjectPath.FindChar(TEXT('.'), DotIndex))
		{
			MeshObjectPath.LeftInline(DotIndex);
		}

		const FString MeshFolder = FPaths::GetPath(MeshObjectPath);
		const bool bIsAssetsFbxOutfitMesh =
			MeshObjectPath.Contains(TEXT("/AssetsFBX/")) &&
			MeshObjectPath.Contains(TEXT("/Outfit/"));
		if (!MeshFolder.EndsWith(TEXT("/SkeletalMeshes")) && !bIsAssetsFbxOutfitMesh)
		{
			return nullptr;
		}

		const FString MeshOwnerFolder = FPaths::GetPath(MeshFolder);
		const FString MaterialName = CurrentMaterial ? CurrentMaterial->GetName() : FString();
		if (bIsAssetsFbxOutfitMesh)
		{
			// 의상 FBX 메쉬에는 이미 원본 /Assets/Pal 머티리얼 슬롯이 들어 있다.
			// 같은 이름의 /AssetsFBX 로컬 머티리얼은 추출 중 깨진 경우가 있어 덮어쓰지 않는다.
			return nullptr;
		}

		const auto LoadLocalMaterialByName = [&MeshOwnerFolder](const FString& CandidateName) -> UMaterialInterface*
		{
			if (CandidateName.IsEmpty())
			{
				return nullptr;
			}

			const FString LocalMaterialPath = FString::Printf(
				TEXT("%s/Materials/%s.%s"),
				*MeshOwnerFolder,
				*CandidateName,
				*CandidateName);
			return LoadObject<UMaterialInterface>(nullptr, *LocalMaterialPath);
		};
		const auto LoadSourceOutfitMaterialByName =
			[&MeshFolder](const FString& CandidateName, const FString& VersionFolder) -> UMaterialInterface*
		{
			if (CandidateName.IsEmpty() || VersionFolder.IsEmpty())
			{
				return nullptr;
			}

			const FString VariantFolder = FPaths::GetPath(MeshFolder);
			const FString OutfitRootFolder = FPaths::GetPath(VariantFolder);
			const FString SourceMaterialPath = FString::Printf(
				TEXT("%s/%s/%s.%s"),
				*OutfitRootFolder,
				*VersionFolder,
				*CandidateName,
				*CandidateName);
			return LoadObject<UMaterialInterface>(nullptr, *SourceMaterialPath);
		};

		FString MeshMaterialStem = Mesh->GetName();
		MeshMaterialStem.RemoveFromStart(TEXT("SK_"));
		if (MeshMaterialStem.EndsWith(TEXT("_2")))
		{
			MeshMaterialStem.LeftChopInline(2);
		}

		if (MeshMaterialStem.Contains(TEXT("Hair")))
		{
			// 헤어는 메쉬 기본 슬롯이 원본이다. 옷 보정 로직으로 덮으면 색과 윤곽이 틀어진다.
			return nullptr;
		}

		const bool bSingleOutfitSlotUsesBodyMaterial =
			MaterialIndex == 0 &&
			MaterialCount == 1 &&
			MaterialName.Contains(TEXT("Body")) &&
			Mesh->GetName().Contains(TEXT("Outfit"));
		if (bSingleOutfitSlotUsesBodyMaterial)
		{
			// 단일 슬롯 의상이 몸 피부 머티리얼을 물고 있으면 같은 의상 폴더의 원본 M01을 찾아 쓴다.
			FString OutfitMaterialStem = MeshMaterialStem;
			if (!OutfitMaterialStem.Contains(TEXT("_v")))
			{
				OutfitMaterialStem += TEXT("_v01");
			}
			FString VersionFolder = TEXT("v01");
			const int32 VersionIndex = OutfitMaterialStem.Find(TEXT("_v"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (VersionIndex != INDEX_NONE && OutfitMaterialStem.Len() >= VersionIndex + 4)
			{
				VersionFolder = OutfitMaterialStem.Mid(VersionIndex + 1, 3);
			}
			OutfitMaterialStem = TEXT("MI_") + OutfitMaterialStem;
			if (UMaterialInterface* OutfitMaterial = LoadSourceOutfitMaterialByName(
				OutfitMaterialStem + TEXT("_M01"),
				VersionFolder))
			{
				return OutfitMaterial;
			}
			if (UMaterialInterface* OutfitMaterial = LoadLocalMaterialByName(OutfitMaterialStem + TEXT("_M01")))
			{
				return OutfitMaterial;
			}
		}

		return nullptr;
	}

	inline void ApplyPreviewOptionMaterials(
		USkeletalMeshComponent* Component,
		const FUEHHVCustomizationOption& Option,
		EUEHHVGender Gender)
	{
		return;
		const int32 MaterialCount = 0;
		/*
		const int32 MaterialCount = FMath::Min(Component->GetNumMaterials(), Materials.Num());
		for (int32 Index = 0; Index < MaterialCount; ++Index)
		{
			UMaterialInterface* Material = Materials[Index].Get();
			if (!Material)
			{
				continue;
			}

			// 의상별 원본 머티리얼은 데이터 에셋에 넣고, 프리뷰 코드는 슬롯 순서대로 적용만 한다.
			EnsurePreviewSkeletalMaterialUsage(Material);
			Component->SetMaterial(Index, Material);
		}
		*/
	}

}


