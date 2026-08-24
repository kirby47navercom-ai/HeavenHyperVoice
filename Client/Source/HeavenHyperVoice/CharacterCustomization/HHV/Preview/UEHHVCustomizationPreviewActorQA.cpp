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

void AUEHHVCustomizationPreviewActor::ApplyQACommandLineAppearance()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("HHVQATypeB")))
	{
		Appearance.Gender = EUEHHVGender::TypeB;
		Appearance.BodyIndex = 2;
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("HHVQATypeA")))
	{
		Appearance.Gender = EUEHHVGender::TypeA;
		Appearance.BodyIndex = 1;
	}

	FString Value;
	if (FParse::Value(FCommandLine::Get(), TEXT("HHVQAHead="), Value))
	{
		Appearance.HeadIndex = FCString::Atoi(*Value);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("HHVQAHair="), Value))
	{
		Appearance.HairIndex = FCString::Atoi(*Value);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("HHVQAEyes="), Value))
	{
		Appearance.EyeIndex = FCString::Atoi(*Value);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("HHVQAOutfit="), Value))
	{
		Appearance.BodyEquipmentIndex = FCString::Atoi(*Value);
	}
	// 체격 스케일은 제거했으므로 QA 커맨드라인 값도 적용하지 않는다.
	Appearance.TorsoVolume = 0.0f;
	Appearance.ArmVolume = 0.0f;
	Appearance.LegVolume = 0.0f;
}

void AUEHHVCustomizationPreviewActor::CaptureQAWidgetScreenshot()
{
	// UI 구조 검증 때는 실제 화면을 캡처해서 UMG와 함께 확인한다.
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		TEXT("Screenshots/Customization/HHV_Widget_UI.png");
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	GetWorldTimerManager().SetTimer(QAExitTimer, this, &ThisClass::ExitAfterQAScreenshot, 1.5f, false);
}

void AUEHHVCustomizationPreviewActor::CaptureQAScreenshot()
{
	UWidgetLayoutLibrary::RemoveAllWidgets(GetWorld());
	TArray<UUserWidget*> QAWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), QAWidgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* Widget : QAWidgets)
	{
		if (Widget)
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
			Widget->RemoveFromParent();
		}
	}
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		UPrimitiveComponent* Component = *It;
		if (!Component || Component->GetWorld() != GetWorld())
		{
			continue;
		}
		if (Component == BaseBodyMesh || Component == BodyEquipmentMesh || Component == HeadMesh || Component == HairMesh)
		{
			continue;
		}

		Component->SetHiddenInGame(true, true);
		Component->SetVisibility(false, true);
	}

	auto LogPart = [](const TCHAR* Name, USkeletalMeshComponent* Component)
	{
		USkeletalMesh* Mesh = Component ? Component->GetSkeletalMeshAsset() : nullptr;
		UE_LOG(LogTemp, Display,
			TEXT("HHV customization QA part %s mesh=%s visible=%d materials=%d torsoMin=%.2f torsoMax=%.2f armMin=%.2f armMax=%.2f legMin=%.2f legMax=%.2f bounds=%s"),
			Name,
			*GetPathNameSafe(Mesh),
			Component && Component->IsVisible(),
			Component ? Component->GetNumMaterials() : 0,
			Component ? Component->GetMorphTarget(TEXT("BS_Torso_min")) : 0.0f,
			Component ? Component->GetMorphTarget(TEXT("BS_Torso_max")) : 0.0f,
			Component ? Component->GetMorphTarget(TEXT("BS_Arm_min")) : 0.0f,
			Component ? Component->GetMorphTarget(TEXT("BS_Arm_max")) : 0.0f,
			Component ? Component->GetMorphTarget(TEXT("BS_Leg_min")) : 0.0f,
			Component ? Component->GetMorphTarget(TEXT("BS_Leg_max")) : 0.0f,
			Component ? *Component->Bounds.GetBox().ToString() : TEXT("none"));
	};

	LogPart(TEXT("BaseBody"), BaseBodyMesh);
	LogPart(TEXT("BodyEquipment"), BodyEquipmentMesh);
	LogPart(TEXT("Head"), HeadMesh);
	LogPart(TEXT("Hair"), HairMesh);

	UE_LOG(LogTemp, Display,
		TEXT("HHV customization QA selection gender=%s body=%d head=%d hair=%d eyes=%d outfit=%d torso=%.2f arm=%.2f leg=%.2f"),
		Appearance.Gender == EUEHHVGender::TypeA ? TEXT("TypeA") : TEXT("TypeB"),
		Appearance.BodyIndex,
		Appearance.HeadIndex,
		Appearance.HairIndex,
		Appearance.EyeIndex,
		Appearance.BodyEquipmentIndex,
		Appearance.TorsoVolume,
		Appearance.ArmVolume,
		Appearance.LegVolume);

	const TCHAR* GenderName = Appearance.Gender == EUEHHVGender::TypeA ? TEXT("TypeA") : TEXT("TypeB");
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		FString::Printf(
			TEXT("Screenshots/Customization/HHV_%s_H%02d_Hair%02d_Eye%02d_Outfit%02d_T%+.1f_A%+.1f_L%+.1f_Full.png"),
			GenderName,
			Appearance.HeadIndex,
			Appearance.HairIndex,
			Appearance.EyeIndex,
			Appearance.BodyEquipmentIndex,
			Appearance.TorsoVolume,
			Appearance.ArmVolume,
			Appearance.LegVolume);
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	GetWorldTimerManager().SetTimer(QAPrepareHeadTimer, this, &ThisClass::PrepareQAHeadScreenshot, 1.0f, false);
}

void AUEHHVCustomizationPreviewActor::PrepareQAHeadScreenshot()
{
	const FVector HeadCenter = HeadMesh
		? SceneRoot->GetComponentTransform().InverseTransformPosition(HeadMesh->Bounds.Origin)
		: FVector(0.0f, 0.0f, 150.0f);
	PreviewCamera->SetRelativeLocation(FVector(0.0f, 95.0f, HeadCenter.Z + 1.0f));
	PreviewCamera->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	PreviewCamera->FieldOfView = 32.0f;
	GetWorldTimerManager().SetTimer(QAHeadScreenshotTimer, this, &ThisClass::CaptureQAHeadScreenshot, 1.0f, false);
}

void AUEHHVCustomizationPreviewActor::CaptureQAHeadScreenshot()
{
	const TCHAR* GenderName = Appearance.Gender == EUEHHVGender::TypeA ? TEXT("TypeA") : TEXT("TypeB");
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		FString::Printf(
			TEXT("Screenshots/Customization/HHV_%s_H%02d_Hair%02d_Eye%02d_Outfit%02d_T%+.1f_A%+.1f_L%+.1f_Head.png"),
			GenderName,
			Appearance.HeadIndex,
			Appearance.HairIndex,
			Appearance.EyeIndex,
			Appearance.BodyEquipmentIndex,
			Appearance.TorsoVolume,
			Appearance.ArmVolume,
			Appearance.LegVolume);
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	GetWorldTimerManager().SetTimer(QAExitTimer, this, &ThisClass::ExitAfterQAScreenshot, 1.5f, false);
}

void AUEHHVCustomizationPreviewActor::ExitAfterQAScreenshot()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0))
		{
			PlayerController->ConsoleCommand(TEXT("quit"), true);
		}
	}
	FPlatformMisc::RequestExit(false);
}

void AUEHHVCustomizationPreviewActor::BeginQABatch()
{
	// 에디터 화면을 건드리지 않는 숨김 실행에서 모든 선택지를 연속 검수한다.
	UWidgetLayoutLibrary::RemoveAllWidgets(GetWorld());
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		UPrimitiveComponent* Component = *It;
		if (!Component || Component->GetWorld() != GetWorld())
		{
			continue;
		}
		if (Component == BaseBodyMesh || Component == BodyEquipmentMesh || Component == HeadMesh || Component == HairMesh)
		{
			continue;
		}
		Component->SetHiddenInGame(true, true);
		Component->SetVisibility(false, true);
	}

	QABatchGenderIndex = 0;
	QABatchPhase = 0;
	QABatchCaseIndex = PreviewFirstVisibleOutfitIndex;
	PrepareNextQABatchCase();
}

void AUEHHVCustomizationPreviewActor::PrepareNextQABatchCase()
{
	if (QABatchGenderIndex >= 2)
	{
		GetWorldTimerManager().SetTimer(QAExitTimer, this, &ThisClass::ExitAfterQAScreenshot, 1.5f, false);
		return;
	}

	Appearance.Gender = QABatchGenderIndex == 0 ? EUEHHVGender::TypeA : EUEHHVGender::TypeB;
	Appearance.BodyIndex = Appearance.Gender == EUEHHVGender::TypeA ? 1 : 2;
	Appearance.HeadIndex = 0;

	if (QABatchPhase == 0)
	{
		const int32 LastOutfitIndex = FMath::Min(
			GetOptionCount(EUEHHVCustomizationCategory::BodyEquipment) - 1,
			PreviewMaxVisibleOutfits);
		if (QABatchCaseIndex > LastOutfitIndex)
		{
			QABatchPhase = 1;
			QABatchCaseIndex = 0;
			PrepareNextQABatchCase();
			return;
		}
		Appearance.BodyEquipmentIndex = QABatchCaseIndex;
		Appearance.EyeIndex = 0;
		Appearance.HairIndex = 0;
	}
	else if (QABatchPhase == 1)
	{
		if (QABatchCaseIndex >= GetOptionCount(EUEHHVCustomizationCategory::Eyes))
		{
			QABatchPhase = 2;
			QABatchCaseIndex = 0;
			PrepareNextQABatchCase();
			return;
		}
		Appearance.BodyEquipmentIndex = PreviewFirstVisibleOutfitIndex;
		Appearance.EyeIndex = QABatchCaseIndex;
		Appearance.HairIndex = 0;
	}
	else
	{
		if (QABatchCaseIndex >= GetOptionCount(EUEHHVCustomizationCategory::Hair))
		{
			++QABatchGenderIndex;
			QABatchPhase = 0;
			QABatchCaseIndex = PreviewFirstVisibleOutfitIndex;
			PrepareNextQABatchCase();
			return;
		}
		Appearance.BodyEquipmentIndex = PreviewFirstVisibleOutfitIndex;
		Appearance.EyeIndex = 0;
		Appearance.HairIndex = QABatchCaseIndex;
	}

	RefreshMeshes();
	if (QABatchPhase == 0)
	{
		PreviewYawDegrees = 0.0f;
		PreviewZoom = 1.0f;
		PreviewPanPixels = FVector2D::ZeroVector;
		CharacterRoot->SetRelativeRotation(FRotator::ZeroRotator);
		FramePreviewCamera();
	}
	else
	{
		const FVector HeadCenter = HeadMesh
			? SceneRoot->GetComponentTransform().InverseTransformPosition(HeadMesh->Bounds.Origin)
			: FVector(0.0f, 0.0f, 150.0f);
		PreviewCamera->SetRelativeLocation(FVector(0.0f, 95.0f, HeadCenter.Z + 1.0f));
		PreviewCamera->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		PreviewCamera->FieldOfView = 32.0f;
	}

	GetWorldTimerManager().SetTimer(QABatchTimer, this, &ThisClass::CaptureCurrentQABatchCase, 0.45f, false);
}

void AUEHHVCustomizationPreviewActor::CaptureCurrentQABatchCase()
{
	const TCHAR* GenderName = Appearance.Gender == EUEHHVGender::TypeA ? TEXT("TypeA") : TEXT("TypeB");
	FString FileName;
	if (QABatchPhase == 0)
	{
		FileName = FString::Printf(TEXT("HHV_%s_Outfit%02d.png"), GenderName, Appearance.BodyEquipmentIndex);
	}
	else if (QABatchPhase == 1)
	{
		FileName = FString::Printf(TEXT("HHV_%s_Eye%02d.png"), GenderName, Appearance.EyeIndex);
	}
	else
	{
		FileName = FString::Printf(TEXT("HHV_%s_Hair%02d.png"), GenderName, Appearance.HairIndex);
	}

	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("Screenshots/Customization/Batch") / FileName;
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	++QABatchCaseIndex;
	GetWorldTimerManager().SetTimer(QABatchTimer, this, &ThisClass::PrepareNextQABatchCase, 0.45f, false);
}


