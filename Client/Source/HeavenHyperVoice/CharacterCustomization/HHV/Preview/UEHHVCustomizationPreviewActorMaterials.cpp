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

#include "UEHHVCustomizationPreviewActorPrivate.h"

using namespace UEHHVCustomizationPreviewActorPrivate;

void AUEHHVCustomizationPreviewActor::ApplyMaterialColors()
{
	// 원본 의상 색은 건드리지 않고, 캐릭터 색상 항목만 선택적으로 틴트한다.
	if (!Appearance.SkinColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyColorToSlots(HeadMesh, Appearance.SkinColor, {TEXT("Head"), TEXT("Body"), TEXT("Skin")});
		ApplyColorToSlots(BaseBodyMesh, Appearance.SkinColor, {TEXT("Body"), TEXT("Skin")});
		ApplyColorToSlots(BodyEquipmentMesh, Appearance.SkinColor, {TEXT("Body"), TEXT("Skin")});
	}

	FLinearColor EffectiveHairColor = Appearance.HairColor;
	if (EffectiveHairColor.Equals(FLinearColor::White, 0.003f))
	{
		// 저장된 예전 프리뷰는 머리색을 흰색으로 들고 있다.
		// 흰색은 실제 선택색이 아니라 "미지정"으로 보고 카탈로그 기본 머리색을 쓴다.
		EffectiveHairColor = Catalog && !Catalog->HairColors.IsEmpty()
			? Catalog->HairColors[0]
			: FUEHHVAppearance().HairColor;
		Appearance.HairColor = EffectiveHairColor;
	}
	if (!EffectiveHairColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyColorToSlots(HairMesh, EffectiveHairColor, {TEXT("Hair")});
	}
}

void AUEHHVCustomizationPreviewActor::ApplyEyeMaterial(const FUEHHVCustomizationOption& Option)
{
	if (!HeadMesh || !Option.Material)
	{
		return;
	}

	const int32 MaterialCount = HeadMesh->GetNumMaterials();
	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		if (IsEyeIrisMaterialSlot(HeadMesh, Index))
		{
			// 각 눈 프리셋의 원본 머티리얼에는 흰자, 홍채, 동공 정보가 이미 들어 있다.
			// 별도 합성 텍스처를 덮으면 서로 다른 프리셋이 같은 검은 눈으로 보이므로 원본을 그대로 쓴다.
			EnsurePreviewSkeletalMaterialUsage(Option.Material);
			HeadMesh->SetMaterial(Index, Option.Material);
		}
	}
}

bool AUEHHVCustomizationPreviewActor::IsEyeIrisMaterialSlot(
	USkeletalMeshComponent* Component,
	int32 MaterialIndex) const
{
	if (!Component || MaterialIndex < 0 || MaterialIndex >= Component->GetNumMaterials())
	{
		return false;
	}

	const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
	const FString SlotName = SlotNames.IsValidIndex(MaterialIndex)
		? SlotNames[MaterialIndex].ToString().ToLower()
		: FString();
	const USkeletalMesh* Mesh = Component->GetSkeletalMeshAsset();
	const TArray<FSkeletalMaterial>* AssetMaterials = Mesh ? &Mesh->GetMaterials() : nullptr;
	const FSkeletalMaterial* AssetMaterial =
		AssetMaterials && AssetMaterials->IsValidIndex(MaterialIndex) ? &(*AssetMaterials)[MaterialIndex] : nullptr;
	const FString AssetSlotName = AssetMaterial ? AssetMaterial->MaterialSlotName.ToString().ToLower() : FString();
	const UMaterialInterface* DefaultMaterial = AssetMaterial ? AssetMaterial->MaterialInterface : nullptr;
	const FString DefaultMaterialName = DefaultMaterial ? DefaultMaterial->GetName().ToLower() : FString();
	const FString SlotIdentity = SlotName + TEXT(" ") + AssetSlotName;
	const FString MaterialIdentity = DefaultMaterialName;
	const FString Combined = SlotIdentity + TEXT(" ") + MaterialIdentity;

	// 눈 머티리얼은 눈 전용 머티리얼 슬롯만 교체한다.
	// 얼굴, 눈썹, 흰자, 코, 입술, 수염, 하이라이트는 원본 머티리얼을 유지해야 한다.
	const bool bExcluded =
		MaterialIdentity.Contains(TEXT("player_head")) ||
		MaterialIdentity.Contains(TEXT("head")) ||
		MaterialIdentity.Contains(TEXT("skin")) ||
		MaterialIdentity.Contains(TEXT("brow")) ||
		MaterialIdentity.Contains(TEXT("beard")) ||
		MaterialIdentity.Contains(TEXT("mouth")) ||
		MaterialIdentity.Contains(TEXT("lip")) ||
		MaterialIdentity.Contains(TEXT("nose")) ||
		(SlotIdentity.Contains(TEXT("skin")) && !Combined.Contains(TEXT("eye"))) ||
		Combined.Contains(TEXT("brow")) ||
		Combined.Contains(TEXT("beard")) ||
		Combined.Contains(TEXT("mustache")) ||
		Combined.Contains(TEXT("moustache")) ||
		Combined.Contains(TEXT("lash")) ||
		Combined.Contains(TEXT("eyelash")) ||
		Combined.Contains(TEXT("lid")) ||
		Combined.Contains(TEXT("eyelid")) ||
		Combined.Contains(TEXT("mouth")) ||
		Combined.Contains(TEXT("nose")) ||
		Combined.Contains(TEXT("lip")) ||
		Combined.Contains(TEXT("teeth")) ||
		Combined.Contains(TEXT("tongue")) ||
		Combined.Contains(TEXT("line")) ||
		Combined.Contains(TEXT("white")) ||
		Combined.Contains(TEXT("sclera")) ||
		Combined.Contains(TEXT("highlight")) ||
		Combined.Contains(TEXT("hi_light"));
	if (bExcluded)
	{
		return false;
	}

	const bool bLooksLikeEyeSlot =
		SlotIdentity.Contains(TEXT("mi_player_eye")) ||
		SlotIdentity.Contains(TEXT("player_eye")) ||
		SlotIdentity.Contains(TEXT("_eye")) ||
		SlotIdentity.Contains(TEXT("iris")) ||
		SlotIdentity.Contains(TEXT("pupil"));
	const bool bHasHHVEyeMaterial =
		MaterialIdentity.Contains(TEXT("mi_player_eye")) ||
		MaterialIdentity.Contains(TEXT("player_eye")) ||
		MaterialIdentity.Contains(TEXT("iris")) ||
		MaterialIdentity.Contains(TEXT("pupil"));
	return bLooksLikeEyeSlot && bHasHHVEyeMaterial;
}

void AUEHHVCustomizationPreviewActor::ApplyColorToSlots(
	USkeletalMeshComponent* Component,
	const FLinearColor& Color,
	const TArray<FString>& SlotContains)
{
	if (!Component || Color.Equals(FLinearColor::White, 0.003f))
	{
		return;
	}

	const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
	const int32 MaterialCount = Component->GetNumMaterials();
	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		const FName SlotName = SlotNames.IsValidIndex(Index)
			? SlotNames[Index]
			: NAME_None;
		const FString SlotIdentity = SlotName.ToString().ToLower();
		const FString MaterialIdentity = GetPathNameSafe(Component->GetMaterial(Index)).ToLower();
		const FString Combined = SlotIdentity + TEXT(" ") + MaterialIdentity;
		const bool bFaceDetail =
			Combined.Contains(TEXT("eye")) ||
			Combined.Contains(TEXT("iris")) ||
			Combined.Contains(TEXT("pupil")) ||
			Combined.Contains(TEXT("sclera")) ||
			Combined.Contains(TEXT("white")) ||
			Combined.Contains(TEXT("highlight")) ||
			Combined.Contains(TEXT("brow")) ||
			Combined.Contains(TEXT("lash")) ||
			Combined.Contains(TEXT("lid")) ||
			Combined.Contains(TEXT("mouth")) ||
			Combined.Contains(TEXT("nose")) ||
			Combined.Contains(TEXT("lip")) ||
			Combined.Contains(TEXT("teeth")) ||
			Combined.Contains(TEXT("tongue")) ||
			Combined.Contains(TEXT("line")) ||
			Combined.Contains(TEXT("beard")) ||
			Combined.Contains(TEXT("mustache")) ||
			Combined.Contains(TEXT("moustache"));
		if (bFaceDetail)
		{
			continue;
		}

		bool bMatches = SlotContains.Num() == 0;
		for (const FString& Token : SlotContains)
		{
			if (Combined.Contains(Token.ToLower()))
			{
				bMatches = true;
				break;
			}
		}
		if (!bMatches)
		{
			continue;
		}

		UMaterialInstanceDynamic* DynamicMaterial = Component->CreateDynamicMaterialInstance(Index);
		if (DynamicMaterial)
		{
			for (const FName ParameterName : {TEXT("TintColor"), TEXT("Color"), TEXT("BaseColor"), TEXT("Base Color"), TEXT("MainColor"), TEXT("HairColor"), TEXT("Hair Color")})
			{
				DynamicMaterial->SetVectorParameterValue(ParameterName, Color);
			}
		}
	}
}

void AUEHHVCustomizationPreviewActor::ResetComponentMaterials(USkeletalMeshComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// 저장된 MID override를 제거해서 선택한 메시의 원본 머티리얼 슬롯을 다시 살린다.
	const int32 MaterialCount = Component->GetNumMaterials();
	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		Component->SetMaterial(Index, nullptr);
		EnsurePreviewSkeletalMaterialUsage(Component->GetMaterial(Index));
	}
}

void AUEHHVCustomizationPreviewActor::ApplyMeshLocalMaterials(USkeletalMeshComponent* Component)
{
	if (!Component || !Component->GetSkeletalMeshAsset())
	{
		return;
	}

	USkeletalMesh* Mesh = Component->GetSkeletalMeshAsset();
	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* CurrentMaterial = Component->GetMaterial(Index);
		EnsurePreviewSkeletalMaterialUsage(CurrentMaterial);
		if (UMaterialInterface* LocalMaterial = LoadMeshLocalMaterial(
			Mesh,
			CurrentMaterial,
			Index,
			Component->GetNumMaterials()))
		{
			// 같은 메쉬 폴더의 원본 머티리얼이 있으면 그쪽을 우선 사용한다.
			EnsurePreviewSkeletalMaterialUsage(LocalMaterial);
			Component->SetMaterial(Index, LocalMaterial);
		}
	}
}

void AUEHHVCustomizationPreviewActor::ApplyMorphSafeMaterials(USkeletalMeshComponent* Component)
{
	if (!Component || MorphSafeMaterialFolder.IsEmpty())
	{
		return;
	}

	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* OriginalMaterial = Component->GetMaterial(Index);
		const FString SafeName = MakeMorphSafeMaterialName(OriginalMaterial);
		if (SafeName.IsEmpty())
		{
			continue;
		}

		const FString SafePath = FString::Printf(TEXT("%s/%s.%s"), *MorphSafeMaterialFolder, *SafeName, *SafeName);
		UMaterialInterface* SafeMaterial = LoadObject<UMaterialInterface>(nullptr, *SafePath);
		if (SafeMaterial)
		{
			// 원본 머티리얼을 부모로 둔 MorphSafe 인스턴스를 써서, 모프 적용 후 기본 회색 머티리얼로 떨어지는 일을 막는다.
			Component->SetMaterial(Index, SafeMaterial);
		}
	}
}

void AUEHHVCustomizationPreviewActor::HideFaceCoverSections(USkeletalMeshComponent* Component)
{
	if (!Component || !Component->GetSkeletalMeshAsset())
	{
		return;
	}

	for (int32 LODIndex = 0; LODIndex < 8; ++LODIndex)
	{
		Component->ShowAllMaterialSections(LODIndex);
	}

	const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
	for (int32 MaterialIndex = 0; MaterialIndex < SlotNames.Num(); ++MaterialIndex)
	{
		const FString Slot = SlotNames[MaterialIndex].ToString().ToLower();
		const UMaterialInterface* Material = Component->GetMaterial(MaterialIndex);
		const FString MaterialPath = GetPathNameSafe(Material).ToLower();
		const FString Identity = Slot + TEXT(" ") + MaterialPath;
		const bool bIsFaceCover =
			Identity.Contains(TEXT("mask")) ||
			Identity.Contains(TEXT("facecover")) ||
			Identity.Contains(TEXT("face_cover")) ||
			Identity.Contains(TEXT("facemask")) ||
			Identity.Contains(TEXT("face_mask")) ||
			Identity.Contains(TEXT("mouthcover")) ||
			Identity.Contains(TEXT("mouth_cover")) ||
			Identity.Contains(TEXT("nosecover")) ||
			Identity.Contains(TEXT("nose_cover")) ||
			Identity.Contains(TEXT("headcover")) ||
			Identity.Contains(TEXT("head_cover")) ||
			Identity.Contains(TEXT("headequ")) ||
			Identity.Contains(TEXT("head_equip")) ||
			Identity.Contains(TEXT("equip_head"));

		if (!bIsFaceCover)
		{
			continue;
		}

		// 얼굴을 덮는 장비 섹션은 커스터마이징 프리뷰에서 제외한다.
		SetPreviewMaterialShownOnAllLods(Component, MaterialIndex, false);
	}
}

void AUEHHVCustomizationPreviewActor::HideBaseBodyOutfitSections(USkeletalMeshComponent* Component)
{
	if (!Component || !Component->GetSkeletalMeshAsset())
	{
		return;
	}

	const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
	for (int32 MaterialIndex = 0; MaterialIndex < SlotNames.Num(); ++MaterialIndex)
	{
		const FString Slot = SlotNames[MaterialIndex].ToString().ToLower();
		const FString MaterialPath = GetPathNameSafe(Component->GetMaterial(MaterialIndex)).ToLower();
		const FString Identity = Slot + TEXT(" ") + MaterialPath;
		const bool bIsSkinSection =
			Identity.Contains(TEXT("body")) ||
			Identity.Contains(TEXT("skin")) ||
			Identity.Contains(TEXT("player_female_body")) ||
			Identity.Contains(TEXT("player_male_body"));
		const bool bIsOutfitSection =
			Identity.Contains(TEXT("outfit")) ||
			Identity.Contains(TEXT("oldcloth")) ||
			Identity.Contains(TEXT("cloth")) ||
			Identity.Contains(TEXT("armor")) ||
			Identity.Contains(TEXT("shirt")) ||
			Identity.Contains(TEXT("pants")) ||
			Identity.Contains(TEXT("shoe")) ||
			Identity.Contains(TEXT("boot"));

		if (!bIsOutfitSection || bIsSkinSection)
		{
			continue;
		}

		// 기본 바디가 가진 기본 옷 슬롯만 끄고, 피부/팔/다리는 의상 빈 부분을 메우도록 남긴다.
		SetPreviewMaterialShownOnAllLods(Component, MaterialIndex, false);
	}
}

void AUEHHVCustomizationPreviewActor::HideEquipmentSkinSections(USkeletalMeshComponent* Component)
{
	if (!Component || !Component->GetSkeletalMeshAsset())
	{
		return;
	}

	const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
	for (int32 MaterialIndex = 0; MaterialIndex < SlotNames.Num(); ++MaterialIndex)
	{
		const FString Slot = SlotNames[MaterialIndex].ToString().ToLower();
		const FString MaterialPath = GetPathNameSafe(Component->GetMaterial(MaterialIndex)).ToLower();
		const FString Identity = Slot + TEXT(" ") + MaterialPath;
		const bool bIsSkinSection =
			Identity.Contains(TEXT("body")) ||
			Identity.Contains(TEXT("skin")) ||
			Identity.Contains(TEXT("player_female_body")) ||
			Identity.Contains(TEXT("player_male_body"));
		if (!bIsSkinSection)
		{
			continue;
		}

		// 의상에 포함된 피부 섹션은 베이스 몸과 중복되어 피부가 옷 위로 뚫린 것처럼 보인다.
		SetPreviewMaterialShownOnAllLods(Component, MaterialIndex, false);
	}
}


