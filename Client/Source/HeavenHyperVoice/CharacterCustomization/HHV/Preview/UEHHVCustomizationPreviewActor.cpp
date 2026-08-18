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

AUEHHVCustomizationPreviewActor::AUEHHVCustomizationPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CharacterRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CharacterRoot"));
	CharacterRoot->SetupAttachment(SceneRoot);
	CharacterRoot->SetMobility(EComponentMobility::Movable);

	BaseBodyMesh = CreateSkeletalPart(this, CharacterRoot, TEXT("BaseBody"));
	BodyEquipmentMesh = CreateSkeletalPart(this, CharacterRoot, TEXT("BodyEquipment"));
	HeadMesh = CreateSkeletalPart(this, CharacterRoot, TEXT("Head"));
	HairMesh = CreateSkeletalPart(this, CharacterRoot, TEXT("Hair"));

	PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
	PreviewCamera->SetupAttachment(SceneRoot);
	PreviewCamera->SetMobility(EComponentMobility::Movable);
	PreviewCamera->SetRelativeLocation(FVector(0.0f, 520.0f, 125.0f));
	PreviewCamera->SetRelativeRotation(FRotator(-3.0f, -90.0f, 0.0f));
	PreviewCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	PreviewCamera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PreviewCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
	PreviewCamera->PostProcessSettings.AutoExposureBias = 2.4f;
	PreviewCamera->SetAutoActivate(true);

	KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetRelativeLocation(FVector(-130.0f, 240.0f, 230.0f));
	KeyLight->SetRelativeRotation(FRotator(-42.0f, -70.0f, 0.0f));
	KeyLight->SetIntensity(160000.0f);
	KeyLight->SetAttenuationRadius(1200.0f);
	KeyLight->SetLightColor(FLinearColor::White);
	KeyLight->SetCastShadows(false);
	KeyLight->SetVolumetricScatteringIntensity(0.0f);

	PreviewDirectionalLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("PreviewDirectionalLight"));
	PreviewDirectionalLight->SetupAttachment(SceneRoot);
	PreviewDirectionalLight->SetMobility(EComponentMobility::Movable);
	PreviewDirectionalLight->SetRelativeRotation(FRotator(-18.0f, -90.0f, 0.0f));
	PreviewDirectionalLight->SetIntensity(8.0f);
	PreviewDirectionalLight->SetCastShadows(false);
	PreviewDirectionalLight->SetLightColor(FLinearColor::White);
	PreviewDirectionalLight->SetVolumetricScatteringIntensity(0.0f);

	FillLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetIntensity(12.0f);
	FillLight->SetLightColor(FLinearColor::White);

	FrontLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FrontLight"));
	FrontLight->SetupAttachment(SceneRoot);
	FrontLight->SetMobility(EComponentMobility::Movable);
	FrontLight->SetRelativeLocation(FVector(0.0f, 250.0f, 130.0f));
	FrontLight->SetIntensity(280000.0f);
	FrontLight->SetAttenuationRadius(1200.0f);
	FrontLight->SetCastShadows(false);
	FrontLight->SetLightColor(FLinearColor::White);
	FrontLight->SetVolumetricScatteringIntensity(0.0f);

	BodyFillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BodyFillLight"));
	BodyFillLight->SetupAttachment(SceneRoot);
	BodyFillLight->SetMobility(EComponentMobility::Movable);
	BodyFillLight->SetRelativeLocation(FVector(0.0f, 290.0f, 82.0f));
	BodyFillLight->SetIntensity(240000.0f);
	BodyFillLight->SetAttenuationRadius(1200.0f);
	BodyFillLight->SetCastShadows(false);
	BodyFillLight->SetLightColor(FLinearColor::White);
	BodyFillLight->SetVolumetricScatteringIntensity(0.0f);
}

void AUEHHVCustomizationPreviewActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshMeshes();
}

void AUEHHVCustomizationPreviewActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	SetActorRotation(FRotator::ZeroRotator);
	SetActorScale3D(FVector::OneVector);
	PreviewYawDegrees = 0.0f;
	PreviewZoom = 1.0f;
	PreviewPanPixels = FVector2D::ZeroVector;
	// 맵/블루프린트에 예전 프리뷰 값이 저장돼 있어도 커마 화면의 기본 의상은 1번으로 시작한다.
	Appearance.BodyEquipmentIndex = PreviewFirstVisibleOutfitIndex;
	Appearance.TorsoVolume = 0.0f;
	Appearance.ArmVolume = 0.0f;
	Appearance.LegVolume = 0.0f;
	if (CharacterRoot)
	{
		CharacterRoot->SetRelativeRotation(FRotator(0.0f, PreviewYawDegrees, 0.0f));
	}
	ConfigurePreviewLighting();
	ApplyQACommandLineAppearance();
	RefreshMeshes();
	PreparePreviewStage();

	if (FParse::Param(FCommandLine::Get(), TEXT("HHVQAScreenshot")))
	{
		GetWorldTimerManager().SetTimer(QAScreenshotTimer, this, &ThisClass::CaptureQAScreenshot, 8.0f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("HHVQAWidgetScreenshot")))
	{
		GetWorldTimerManager().SetTimer(QAWidgetScreenshotTimer, this, &ThisClass::CaptureQAWidgetScreenshot, 8.0f, false);
	}
}

void AUEHHVCustomizationPreviewActor::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (PreviewCamera)
	{
		PreviewCamera->GetCameraView(DeltaTime, OutResult);
		return;
	}

	Super::CalcCamera(DeltaTime, OutResult);
}

void AUEHHVCustomizationPreviewActor::AddPreviewYaw(float DeltaYaw)
{
	PreviewYawDegrees = FMath::UnwindDegrees(PreviewYawDegrees + DeltaYaw);
	if (CharacterRoot)
	{
		CharacterRoot->SetRelativeRotation(FRotator(0.0f, PreviewYawDegrees, 0.0f));
	}
}

void AUEHHVCustomizationPreviewActor::AddPreviewZoom(float DeltaZoom)
{
	PreviewZoom = FMath::Clamp(PreviewZoom + DeltaZoom, 0.55f, 1.85f);
	FramePreviewCamera();
}

void AUEHHVCustomizationPreviewActor::AddPreviewPan(FVector2D DeltaPixels)
{
	PreviewPanPixels.X = FMath::Clamp(PreviewPanPixels.X + DeltaPixels.X, -260.0f, 260.0f);
	PreviewPanPixels.Y = FMath::Clamp(PreviewPanPixels.Y + DeltaPixels.Y, -180.0f, 180.0f);
	FramePreviewCamera();
}

void AUEHHVCustomizationPreviewActor::ApplyAppearance(const FUEHHVAppearance& NewAppearance)
{
	Appearance = NewAppearance;
	NormalizeLegacyDefaultColors();
	RefreshMeshes();
}

void AUEHHVCustomizationPreviewActor::SelectOption(EUEHHVCustomizationCategory Category, int32 Index)
{
	const int32 ClampedIndex = ClampIndex(Index, GetOptionCount(Category));
	MutableIndex(Category) = ClampedIndex;

	if (Category == EUEHHVCustomizationCategory::Body)
	{
		const FString BodyId = GetOption(Category, ClampedIndex).Id;
		if (BodyId.Equals(TEXT("TypeA"), ESearchCase::IgnoreCase))
		{
			Appearance.Gender = EUEHHVGender::TypeA;
		}
		else if (BodyId.Equals(TEXT("TypeB"), ESearchCase::IgnoreCase))
		{
			Appearance.Gender = EUEHHVGender::TypeB;
		}
	}

	RefreshMeshes();
}

int32 AUEHHVCustomizationPreviewActor::GetOptionCount(EUEHHVCustomizationCategory Category) const
{
	return Catalog ? Catalog->GetOptionCount(Category) : 0;
}

FString AUEHHVCustomizationPreviewActor::GetOptionLabel(
	EUEHHVCustomizationCategory Category,
	int32 Index) const
{
	const FUEHHVCustomizationOption& Option = GetOption(Category, Index);
	return Option.DisplayName.IsEmpty() ? Option.Id : Option.DisplayName;
}

void AUEHHVCustomizationPreviewActor::SelectGender(EUEHHVGender NewGender)
{
	Appearance.Gender = NewGender;
	Appearance.BodyIndex = NewGender == EUEHHVGender::TypeA ? 1 : 2;
	RefreshMeshes();
}

void AUEHHVCustomizationPreviewActor::SetColor(EUEHHVColorChannel Channel, const FLinearColor& Color)
{
	NormalizeLegacyDefaultColors();
	const FLinearColor ClampedColor = Color.GetClamped();
	switch (Channel)
	{
	case EUEHHVColorChannel::Skin:
		Appearance.SkinColor = ClampedColor;
		break;
	case EUEHHVColorChannel::Hair:
		Appearance.HairColor = ClampedColor;
		break;
	case EUEHHVColorChannel::Eye:
		Appearance.EyeColor = ClampedColor;
		break;
	default:
	// 의상 색은 원본 머티리얼과 텍스처를 그대로 사용한다.
		break;
	}
	RefreshMeshes();
}

void AUEHHVCustomizationPreviewActor::RefreshMeshes()
{
	if (!Catalog)
	{
		return;
	}
	NormalizeLegacyDefaultColors();

	Appearance.BodyIndex = ClampIndex(Appearance.BodyIndex, GetOptionCount(EUEHHVCustomizationCategory::Body));
	if (Appearance.BodyIndex == 0 && GetOptionCount(EUEHHVCustomizationCategory::Body) > 2)
	{
		// 0번 Body는 예전 추출 기본값이라 실제 커마에서는 쓰지 않는다.
		// 화면에 보이는 커마 체형 타입은 TypeA=1, TypeB=2부터 시작한다.
		Appearance.BodyIndex = Appearance.Gender == EUEHHVGender::TypeB ? 2 : 1;
	}
	Appearance.HeadIndex = ClampIndex(Appearance.HeadIndex, GetOptionCount(EUEHHVCustomizationCategory::Head));
	Appearance.HairIndex = ClampIndex(Appearance.HairIndex, GetOptionCount(EUEHHVCustomizationCategory::Hair));
	Appearance.EyeIndex = ClampIndex(Appearance.EyeIndex, GetOptionCount(EUEHHVCustomizationCategory::Eyes));
	const int32 BodyEquipmentCount = GetOptionCount(EUEHHVCustomizationCategory::BodyEquipment);
	if (BodyEquipmentCount > PreviewFirstVisibleOutfitIndex)
	{
		// 의상 0번은 추출용 베이스라 제외하고, 실제 선택은 1~14번만 허용한다.
		const int32 LastVisibleOutfitIndex = FMath::Min(BodyEquipmentCount - 1, PreviewMaxVisibleOutfits);
		Appearance.BodyEquipmentIndex = FMath::Clamp(
			Appearance.BodyEquipmentIndex,
			PreviewFirstVisibleOutfitIndex,
			LastVisibleOutfitIndex);
	}
	else
	{
		Appearance.BodyEquipmentIndex = 0;
	}
	// 얼굴을 가리는 장비 섹션은 숨기고 머리 장비 컴포넌트는 사용하지 않는다.
	const FUEHHVCustomizationOption& Body = GetOption(
		EUEHHVCustomizationCategory::Body,
		Appearance.BodyIndex);
	const FUEHHVCustomizationOption& BodyEquipment = GetOption(
		EUEHHVCustomizationCategory::BodyEquipment,
		Appearance.BodyEquipmentIndex);
	const FUEHHVCustomizationOption& Head = GetOption(
		EUEHHVCustomizationCategory::Head,
		Appearance.HeadIndex);
	const FUEHHVCustomizationOption& Hair = GetOption(
		EUEHHVCustomizationCategory::Hair,
		Appearance.HairIndex);
	const FUEHHVCustomizationOption& Eyes = GetOption(
		EUEHHVCustomizationCategory::Eyes,
		Appearance.EyeIndex);
	USkeletalMesh* BaseMesh = Body.LoadMesh(Appearance.Gender);
	USkeletalMesh* EquipmentMesh = BodyEquipment.LoadMesh(Appearance.Gender);
	const bool bSameOutfitAsBase =
		EquipmentMesh == BaseMesh || GetPathNameSafe(EquipmentMesh).Equals(GetPathNameSafe(BaseMesh));
	const bool bUsesSeparateOutfit = EquipmentMesh && !bSameOutfitAsBase && Appearance.BodyEquipmentIndex > 0;
	// 의상 추출 메시는 피부와 하의까지 포함한 완성 바디이므로 하나의 주 메시에 넣는다.
	// 파츠별 스켈레톤으로 나누면 미리보기에서도 몸과 옷 포즈가 어긋난다.
	BaseBodyMesh->SetSkeletalMesh(bUsesSeparateOutfit ? EquipmentMesh : BaseMesh);
	BaseBodyMesh->SetVisibility(true, false);
	BaseBodyMesh->SetHiddenInGame(false, false);
	ResetComponentMaterials(BaseBodyMesh);
	ApplyMeshLocalMaterials(BaseBodyMesh);
	// 원본 의상/피부 텍스처를 그대로 써야 하므로 커마 프리뷰에서는 대체 머티리얼을 덮지 않는다.
	HideFaceCoverSections(BaseBodyMesh);
	BodyEquipmentMesh->SetLeaderPoseComponent(nullptr);
	BodyEquipmentMesh->SetSkeletalMesh(nullptr);
	BodyEquipmentMesh->SetVisibility(false, true);
	BodyEquipmentMesh->SetHiddenInGame(true, true);
	HeadMesh->SetSkeletalMesh(Head.LoadMesh(Appearance.Gender));
	ResetComponentMaterials(HeadMesh);
	ApplyMeshLocalMaterials(HeadMesh);
	HideFaceCoverSections(HeadMesh);
	HairMesh->SetSkeletalMesh(Hair.LoadMesh(Appearance.Gender));
	ResetComponentMaterials(HairMesh);
	ApplyMeshLocalMaterials(HairMesh);

	// 메시 교체 후에도 커마 프리뷰는 정지 포즈를 유지한다.
	// 선택할 때마다 기본 idle 애니메이션이 재생되면 캐릭터가 위아래로 흔들려 보인다.
	for (USkeletalMeshComponent* PreviewPart : {BaseBodyMesh.Get(), BodyEquipmentMesh.Get(), HeadMesh.Get(), HairMesh.Get()})
	{
		if (PreviewPart)
		{
			PreviewPart->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			PreviewPart->bPauseAnims = true;
		}
	}

	RefreshFollowerPose();
	ApplyMaterialColors();
	ApplyEyeMaterial(Eyes);
	ApplyScale();
	FramePreviewCamera();
	HideUnsupportedAttachmentComponents();
}


