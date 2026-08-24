#include "UEHHVCustomizationPreviewActor.h"

#include "../Data/UEHHVCustomizationTypes.h"
#include "../../../Animation/UEFollowerAnimInstance.h"

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

void AUEHHVCustomizationPreviewActor::ConfigurePreviewLighting()
{
	const FVector ActorLocation = GetActorLocation();
	const FVector LookTarget = ActorLocation + FVector(0.0f, 0.0f, 98.0f);

	if (PreviewCamera)
	{
		PreviewCamera->SetRelativeLocation(FVector(0.0f, 520.0f, 128.0f));
		PreviewCamera->SetRelativeRotation(UKismetMathLibrary::FindLookAtRotation(
			PreviewCamera->GetComponentLocation(),
			LookTarget));
		PreviewCamera->FieldOfView = 36.0f;
		PreviewCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
		PreviewCamera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		PreviewCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
		PreviewCamera->PostProcessSettings.AutoExposureBias = 2.4f;
	}

	if (KeyLight)
	{
		KeyLight->SetRelativeLocation(FVector(-160.0f, 240.0f, 220.0f));
		KeyLight->SetRelativeRotation(UKismetMathLibrary::FindLookAtRotation(
			KeyLight->GetComponentLocation(),
			LookTarget));
		KeyLight->SetIntensity(160000.0f);
		KeyLight->SetAttenuationRadius(1200.0f);
		KeyLight->SetOuterConeAngle(90.0f);
		KeyLight->SetInnerConeAngle(70.0f);
		KeyLight->SetCastShadows(false);
		KeyLight->SetLightColor(FLinearColor::White);
		KeyLight->SetVolumetricScatteringIntensity(0.0f);
	}

	if (PreviewDirectionalLight)
	{
		// 몸과 옷의 원본 머티리얼을 유지하고, 프리뷰에서 어둡게 죽지 않도록 정면 조명을 보강한다.
		PreviewDirectionalLight->SetRelativeRotation(FRotator(-28.0f, -90.0f, 0.0f));
		PreviewDirectionalLight->SetIntensity(8.0f);
		PreviewDirectionalLight->SetCastShadows(false);
		PreviewDirectionalLight->SetLightColor(FLinearColor::White);
		PreviewDirectionalLight->SetVolumetricScatteringIntensity(0.0f);
	}

	if (FillLight)
	{
		FillLight->SetIntensity(12.0f);
		FillLight->SetLightColor(FLinearColor::White);
	}

	if (FrontLight)
	{
		FrontLight->SetRelativeLocation(FVector(0.0f, 250.0f, 138.0f));
		FrontLight->SetIntensity(280000.0f);
		FrontLight->SetAttenuationRadius(1200.0f);
		FrontLight->SetCastShadows(false);
		FrontLight->SetLightColor(FLinearColor::White);
		FrontLight->SetVolumetricScatteringIntensity(0.0f);
	}

	if (BodyFillLight)
	{
		// 몸통과 의상이 검게 죽지 않도록 낮은 정면 보조광을 둔다.
		BodyFillLight->SetRelativeLocation(FVector(0.0f, 290.0f, 82.0f));
		BodyFillLight->SetIntensity(240000.0f);
		BodyFillLight->SetAttenuationRadius(1200.0f);
		BodyFillLight->SetCastShadows(false);
		BodyFillLight->SetLightColor(FLinearColor::White);
		BodyFillLight->SetVolumetricScatteringIntensity(0.0f);
	}
}

void AUEHHVCustomizationPreviewActor::FramePreviewCamera()
{
	if (!PreviewCamera)
	{
		return;
	}

	constexpr float FovDegrees = 36.0f;
	constexpr float BaseDistance = 560.0f;
	const float Distance = BaseDistance * PreviewZoom;

	// 머리나 의상을 바꿔도 bounds 중심을 다시 잡지 않는다.
	// 그래야 커마 선택 때 캐릭터가 위아래로 튀어 보이지 않는다.
	FVector Target(0.0f, 0.0f, 98.0f);
	Target += FVector(PreviewPanPixels.X * 0.11f, 0.0f, -PreviewPanPixels.Y * 0.11f);
	const FVector CameraLocation = Target + FVector(0.0f, Distance, 18.0f);

	PreviewCamera->SetWorldLocation(CameraLocation);
	PreviewCamera->SetWorldRotation(UKismetMathLibrary::FindLookAtRotation(CameraLocation, Target));
	PreviewCamera->FieldOfView = FovDegrees;
}

void AUEHHVCustomizationPreviewActor::PreparePreviewStage()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 커마 레벨의 배경과 조명은 레벨/WBP가 맡는다.
	// 프리뷰 메시를 다시 켤 때 이전 자동 숨김 처리가 남아서 화면에 안 나오는 일을 막는다.
	for (USkeletalMeshComponent* Component : {BaseBodyMesh.Get(), BodyEquipmentMesh.Get(), HeadMesh.Get(), HairMesh.Get()})
	{
		if (!Component || Component->GetWorld() != World)
		{
			continue;
		}

		// 실제로 메쉬가 있는 파트만 다시 켠다. 비워 둔 의상 보조 컴포넌트가 화면에 끼어들면 안 된다.
		const bool bHasMesh = Component->GetSkeletalMeshAsset() != nullptr;
		Component->SetHiddenInGame(!bHasMesh, true);
		Component->SetVisibility(bHasMesh, true);
	}
}

void AUEHHVCustomizationPreviewActor::NormalizeLegacyDefaultColors()
{
	// 이전 프리뷰 BP가 저장한 검은/흰 MID override를 제거한다.
	// 아주 어두운 기본값이 단색으로 돌아가지 않게 원본 텍스처를 그대로 보이게 한다.
	const FUEHHVAppearance Defaults;
	if (Appearance.SkinColor.R < 0.01f && Appearance.SkinColor.G < 0.01f && Appearance.SkinColor.B < 0.01f)
	{
		Appearance.SkinColor = Defaults.SkinColor;
	}
	if ((Appearance.HairColor.R < 0.01f && Appearance.HairColor.G < 0.01f && Appearance.HairColor.B < 0.01f) ||
		Appearance.HairColor.Equals(FLinearColor::White, 0.003f))
	{
		Appearance.HairColor = Catalog && !Catalog->HairColors.IsEmpty() ? Catalog->HairColors[0] : Defaults.HairColor;
	}
	if (Appearance.EyeColor.R < 0.01f && Appearance.EyeColor.G < 0.01f && Appearance.EyeColor.B < 0.01f)
	{
		Appearance.EyeColor = Defaults.EyeColor;
	}
}

void AUEHHVCustomizationPreviewActor::RefreshFollowerPose()
{
	for (USkeletalMeshComponent* Follower : {BodyEquipmentMesh.Get(), HeadMesh.Get(), HairMesh.Get()})
	{
		if (!Follower)
		{
			continue;
		}

		USkeletalMesh* FollowerMesh = Follower->GetSkeletalMeshAsset();
		Follower->SetLeaderPoseComponent(nullptr);
		if (FollowerMesh && BaseBodyMesh && BaseBodyMesh->GetSkeletalMeshAsset())
		{
			// 원본 Head/Hair AnimBP와 같은 방식으로 본 이름을 기준으로 몸 포즈를 복사한다.
			// 서로 다른 USkeleton 자산인 파츠에도 안전하게 적용된다.
			Follower->SetEnableAnimation(true);
			Follower->SetAnimationMode(EAnimationMode::AnimationBlueprint, true);
			Follower->SetAnimInstanceClass(UUEFollowerAnimInstance::StaticClass());
			Follower->ReinitializeAnimNodes();
			if (UUEFollowerAnimInstance* FollowerInstance = Cast<UUEFollowerAnimInstance>(Follower->GetAnimInstance()))
			{
				FollowerInstance->SetCopyCurves(Follower == HeadMesh);
				FollowerInstance->SetUseTargetEyeReferencePose(Follower == HeadMesh);
			}
		}
		else
		{
			Follower->SetAnimInstanceClass(nullptr);
			Follower->SetEnableAnimation(false);
		}
		Follower->SetRelativeTransform(FTransform::Identity);
		Follower->SetVisibility(FollowerMesh != nullptr, true);
	}
}

void AUEHHVCustomizationPreviewActor::HideUnsupportedAttachmentComponents()
{
	TArray<USkeletalMeshComponent*> Components;
	GetComponents(Components);

	for (USkeletalMeshComponent* Component : Components)
	{
		if (!Component ||
			Component == BaseBodyMesh ||
			Component == BodyEquipmentMesh ||
			Component == HeadMesh ||
			Component == HairMesh)
		{
			continue;
		}

		const FString Identity = FString::Printf(
			TEXT("%s %s"),
			*Component->GetName(),
			*GetPathNameSafe(Component->GetSkeletalMeshAsset())).ToLower();
		const bool bRemovedAttachment =
			Identity.Contains(TEXT("accessory")) ||
			Identity.Contains(TEXT("headgear")) ||
			Identity.Contains(TEXT("head_gear")) ||
			Identity.Contains(TEXT("headequ")) ||
			Identity.Contains(TEXT("head_equip")) ||
			Identity.Contains(TEXT("equip_head")) ||
			Identity.Contains(TEXT("glasses")) ||
			Identity.Contains(TEXT("mask")) ||
			Identity.Contains(TEXT("facecover")) ||
			Identity.Contains(TEXT("hat")) ||
			Identity.Contains(TEXT("cap"));
		if (!bRemovedAttachment)
		{
			continue;
		}

		// 이번 커마에서는 부착물을 쓰지 않는다.
		// BP에 남은 이전 헤드기어/마스크 컴포넌트가 화면에 나오지 않게 막는다.
		Component->SetLeaderPoseComponent(nullptr);
		Component->SetSkeletalMesh(nullptr);
		Component->SetVisibility(false, true);
		Component->SetHiddenInGame(true, true);
	}
}


