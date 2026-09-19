#pragma once

#include "UEHHVCustomizationPreviewActor.h"
#include "../../../Data/UEProjectAssets.h"

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
		const UUEProjectAssets* Assets = UUEProjectAssetSettings::GetProjectAssets();
		return Assets ? Assets->FindCharacterMaterial(Mesh, MaterialIndex) : nullptr;
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


