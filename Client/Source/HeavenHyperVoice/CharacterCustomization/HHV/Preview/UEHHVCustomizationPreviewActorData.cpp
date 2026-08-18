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

void AUEHHVCustomizationPreviewActor::ApplyScale()
{
	// 체격 스케일 기능은 제거했다. 저장/QA에 남은 값도 항상 기본 체형으로 되돌린다.
	Appearance.TorsoVolume = 0.0f;
	Appearance.ArmVolume = 0.0f;
	Appearance.LegVolume = 0.0f;

	// 루트와 부위 스케일은 항상 1로 고정한다.
	CharacterRoot->SetRelativeScale3D(FVector::OneVector);

	for (USkeletalMeshComponent* Component : {BaseBodyMesh.Get(), BodyEquipmentMesh.Get(), HeadMesh.Get(), HairMesh.Get()})
	{
		if (Component)
		{
			Component->SetRelativeScale3D(FVector::OneVector);
			Component->SetRelativeLocation(FVector::ZeroVector);
		}
	}

	// 이전 실행에서 남은 체형 모프가 있으면 0으로 지운다.
	ApplyPreviewSignedMorphTarget(BaseBodyMesh, TEXT("BS_Torso_min"), TEXT("BS_Torso_max"), 0.0f);
	ApplyPreviewSignedMorphTarget(BaseBodyMesh, TEXT("BS_Arm_min"), TEXT("BS_Arm_max"), 0.0f);
	ApplyPreviewSignedMorphTarget(BaseBodyMesh, TEXT("BS_Leg_min"), TEXT("BS_Leg_max"), 0.0f);
	ApplyPreviewSignedMorphTarget(BodyEquipmentMesh, TEXT("BS_Torso_min"), TEXT("BS_Torso_max"), 0.0f);
	ApplyPreviewSignedMorphTarget(BodyEquipmentMesh, TEXT("BS_Arm_min"), TEXT("BS_Arm_max"), 0.0f);
	ApplyPreviewSignedMorphTarget(BodyEquipmentMesh, TEXT("BS_Leg_min"), TEXT("BS_Leg_max"), 0.0f);
}

const FUEHHVCustomizationOption& AUEHHVCustomizationPreviewActor::GetOption(
	EUEHHVCustomizationCategory Category,
	int32 Index) const
{
	return Catalog ? Catalog->GetOption(Category, Index) : PreviewEmptyOption;
}

int32& AUEHHVCustomizationPreviewActor::MutableIndex(EUEHHVCustomizationCategory Category)
{
	switch (Category)
	{
	case EUEHHVCustomizationCategory::Body:
		return Appearance.BodyIndex;
	case EUEHHVCustomizationCategory::Head:
		return Appearance.HeadIndex;
	case EUEHHVCustomizationCategory::Hair:
		return Appearance.HairIndex;
	case EUEHHVCustomizationCategory::Eyes:
		return Appearance.EyeIndex;
	case EUEHHVCustomizationCategory::BodyEquipment:
		return Appearance.BodyEquipmentIndex;
	default:
		return Appearance.BodyIndex;
	}
}

int32 AUEHHVCustomizationPreviewActor::GetIndex(EUEHHVCustomizationCategory Category) const
{
	switch (Category)
	{
	case EUEHHVCustomizationCategory::Body:
		return Appearance.BodyIndex;
	case EUEHHVCustomizationCategory::Head:
		return Appearance.HeadIndex;
	case EUEHHVCustomizationCategory::Hair:
		return Appearance.HairIndex;
	case EUEHHVCustomizationCategory::Eyes:
		return Appearance.EyeIndex;
	case EUEHHVCustomizationCategory::BodyEquipment:
		return Appearance.BodyEquipmentIndex;
	default:
		return 0;
	}
}

int32 AUEHHVCustomizationPreviewActor::ClampIndex(int32 Index, int32 Count)
{
	if (Count <= 0)
	{
		return 0;
	}
	return FMath::Clamp(Index, 0, Count - 1);
}

