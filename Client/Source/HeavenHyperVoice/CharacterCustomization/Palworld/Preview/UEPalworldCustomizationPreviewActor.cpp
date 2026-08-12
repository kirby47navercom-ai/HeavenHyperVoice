#include "UEPalworldCustomizationPreviewActor.h"

#include "../Data/UEPalworldCustomizationTypes.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Scene.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
	const FUEPalworldCustomizationOption PreviewEmptyOption;

	bool HasMatchingReferenceSkeleton(const USkeletalMesh* Left, const USkeletalMesh* Right)
	{
		if (!Left || !Right)
		{
			return false;
		}

		const FReferenceSkeleton& LeftSkeleton = Left->GetRefSkeleton();
		const FReferenceSkeleton& RightSkeleton = Right->GetRefSkeleton();
		const int32 BoneCount = LeftSkeleton.GetRawBoneNum();
		if (BoneCount == 0 || BoneCount != RightSkeleton.GetRawBoneNum())
		{
			return false;
		}

		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			if (LeftSkeleton.GetBoneName(BoneIndex) != RightSkeleton.GetBoneName(BoneIndex))
			{
				return false;
			}
		}
		return true;
	}

	USkeletalMeshComponent* CreateSkeletalPart(AActor* Owner, USceneComponent* Parent, const FName& Name)
	{
		USkeletalMeshComponent* Component = Owner->CreateDefaultSubobject<USkeletalMeshComponent>(Name);
		Component->SetupAttachment(Parent);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->bReceivesDecals = false;
		Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		return Component;
	}
}

AUEPalworldCustomizationPreviewActor::AUEPalworldCustomizationPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CharacterRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CharacterRoot"));
	CharacterRoot->SetupAttachment(SceneRoot);

	BodyEquipmentMesh = CreateSkeletalPart(this, CharacterRoot, TEXT("BodyEquipment"));
	HeadMesh = CreateSkeletalPart(this, CharacterRoot, TEXT("Head"));
	HairMesh = CreateSkeletalPart(this, CharacterRoot, TEXT("Hair"));

	PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
	PreviewCamera->SetupAttachment(SceneRoot);
	PreviewCamera->SetRelativeLocation(FVector(0.0f, 230.0f, 125.0f));
	PreviewCamera->SetRelativeRotation(FRotator(-2.0f, -90.0f, 0.0f));
	PreviewCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	PreviewCamera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PreviewCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
	PreviewCamera->PostProcessSettings.AutoExposureBias = 3.2f;

	KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetRelativeLocation(FVector(80.0f, 210.0f, 230.0f));
	KeyLight->SetRelativeRotation(FRotator(-42.0f, -115.0f, 0.0f));
	KeyLight->SetIntensity(42000.0f);
	KeyLight->SetAttenuationRadius(600.0f);
	KeyLight->SetLightColor(FLinearColor::White);
	KeyLight->SetCastShadows(false);

	FillLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetIntensity(9.0f);
	FillLight->SetLightColor(FLinearColor::White);

	FrontLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FrontLight"));
	FrontLight->SetupAttachment(SceneRoot);
	FrontLight->SetRelativeLocation(FVector(0.0f, 90.0f, 130.0f));
	FrontLight->SetIntensity(30000.0f);
	FrontLight->SetAttenuationRadius(650.0f);
	FrontLight->SetCastShadows(false);
	FrontLight->SetLightColor(FLinearColor::White);
}

void AUEPalworldCustomizationPreviewActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshMeshes();
}

void AUEPalworldCustomizationPreviewActor::BeginPlay()
{
	Super::BeginPlay();
	ConfigurePreviewLighting();
	ApplyQACommandLineAppearance();
	RefreshMeshes();

	if (FParse::Param(FCommandLine::Get(), TEXT("PalworldQAScreenshot")))
	{
		GetWorldTimerManager().SetTimer(QAScreenshotTimer, this, &ThisClass::CaptureQAScreenshot, 8.0f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("PalworldQAWidgetScreenshot")))
	{
		GetWorldTimerManager().SetTimer(QAWidgetScreenshotTimer, this, &ThisClass::CaptureQAWidgetScreenshot, 8.0f, false);
	}
}

void AUEPalworldCustomizationPreviewActor::ApplyAppearance(const FUEPalworldAppearance& NewAppearance)
{
	Appearance = NewAppearance;
	NormalizeLegacyDefaultColors();
	RefreshMeshes();
}

void AUEPalworldCustomizationPreviewActor::SelectOption(EUEPalworldCustomizationCategory Category, int32 Index)
{
	const int32 ClampedIndex = ClampIndex(Index, GetOptionCount(Category));
	MutableIndex(Category) = ClampedIndex;

	if (Category == EUEPalworldCustomizationCategory::Body)
	{
		const FString BodyId = GetOption(Category, ClampedIndex).Id;
		if (BodyId.Equals(TEXT("TypeA"), ESearchCase::IgnoreCase))
		{
			Appearance.Gender = EUEPalworldGender::TypeA;
		}
		else if (BodyId.Equals(TEXT("TypeB"), ESearchCase::IgnoreCase))
		{
			Appearance.Gender = EUEPalworldGender::TypeB;
		}
	}

	RefreshMeshes();
}

int32 AUEPalworldCustomizationPreviewActor::GetOptionCount(EUEPalworldCustomizationCategory Category) const
{
	return Catalog ? Catalog->GetOptionCount(Category) : 0;
}

FString AUEPalworldCustomizationPreviewActor::GetOptionLabel(
	EUEPalworldCustomizationCategory Category,
	int32 Index) const
{
	const FUEPalworldCustomizationOption& Option = GetOption(Category, Index);
	return Option.DisplayName.IsEmpty() ? Option.Id : Option.DisplayName;
}

void AUEPalworldCustomizationPreviewActor::SelectGender(EUEPalworldGender NewGender)
{
	Appearance.Gender = NewGender;
	Appearance.BodyIndex = NewGender == EUEPalworldGender::TypeA ? 1 : 2;
	RefreshMeshes();
}

void AUEPalworldCustomizationPreviewActor::SetColor(EUEPalworldColorChannel Channel, const FLinearColor& Color)
{
	NormalizeLegacyDefaultColors();
	const FLinearColor ClampedColor = Color.GetClamped();
	switch (Channel)
	{
	case EUEPalworldColorChannel::Skin:
		Appearance.SkinColor = ClampedColor;
		break;
	case EUEPalworldColorChannel::Hair:
		Appearance.HairColor = ClampedColor;
		break;
	case EUEPalworldColorChannel::Eye:
		Appearance.EyeColor = ClampedColor;
		break;
	default:
		// 의상 색은 Palworld 원본 머티리얼과 텍스처를 그대로 사용한다.
		break;
	}
	RefreshMeshes();
}

void AUEPalworldCustomizationPreviewActor::SetScaleValue(EUEPalworldScaleChannel Channel, float Value)
{
	NormalizeLegacyDefaultColors();
	const float ClampedValue = FMath::Clamp(Value, 0.75f, 1.25f);
	const float VolumeValue = FMath::Clamp((ClampedValue - 1.0f) / 0.25f, -1.0f, 1.0f);
	switch (Channel)
	{
	case EUEPalworldScaleChannel::TorsoSize:
		Appearance.TorsoVolume = VolumeValue;
		break;
	case EUEPalworldScaleChannel::ArmSize:
		Appearance.ArmVolume = VolumeValue;
		break;
	case EUEPalworldScaleChannel::LegSize:
		Appearance.LegVolume = VolumeValue;
		break;
	default:
		break;
	}
	RefreshMeshes();
}

void AUEPalworldCustomizationPreviewActor::RefreshMeshes()
{
	if (!Catalog)
	{
		return;
	}
	NormalizeLegacyDefaultColors();

	Appearance.BodyIndex = ClampIndex(Appearance.BodyIndex, GetOptionCount(EUEPalworldCustomizationCategory::Body));
	Appearance.HeadIndex = ClampIndex(Appearance.HeadIndex, GetOptionCount(EUEPalworldCustomizationCategory::Head));
	Appearance.HairIndex = ClampIndex(Appearance.HairIndex, GetOptionCount(EUEPalworldCustomizationCategory::Hair));
	Appearance.EyeIndex = ClampIndex(Appearance.EyeIndex, GetOptionCount(EUEPalworldCustomizationCategory::Eyes));
	Appearance.BodyEquipmentIndex = ClampIndex(Appearance.BodyEquipmentIndex, GetOptionCount(EUEPalworldCustomizationCategory::BodyEquipment));
	// 얼굴을 가리는 장비성 섹션은 숨기고, 머리장비 컴포넌트는 사용하지 않는다.
	const FUEPalworldCustomizationOption& BodyEquipment = GetOption(
		EUEPalworldCustomizationCategory::BodyEquipment,
		Appearance.BodyEquipmentIndex);
	const FUEPalworldCustomizationOption& Head = GetOption(
		EUEPalworldCustomizationCategory::Head,
		Appearance.HeadIndex);
	const FUEPalworldCustomizationOption& Hair = GetOption(
		EUEPalworldCustomizationCategory::Hair,
		Appearance.HairIndex);
	const FUEPalworldCustomizationOption& Eyes = GetOption(
		EUEPalworldCustomizationCategory::Eyes,
		Appearance.EyeIndex);
	BodyEquipmentMesh->SetSkeletalMesh(BodyEquipment.LoadMesh(Appearance.Gender));
	ResetComponentMaterials(BodyEquipmentMesh);
	HideFaceCoverSections(BodyEquipmentMesh);
	HeadMesh->SetSkeletalMesh(Head.LoadMesh(Appearance.Gender));
	ResetComponentMaterials(HeadMesh);
	HideFaceCoverSections(HeadMesh);
	HairMesh->SetSkeletalMesh(Hair.LoadMesh(Appearance.Gender));
	ResetComponentMaterials(HairMesh);

	RefreshFollowerPose();
	ApplyEyeMaterial(Eyes);
	ApplyMaterialColors();
	ApplyScale();
}

void AUEPalworldCustomizationPreviewActor::ApplyQACommandLineAppearance()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("PalworldQATypeB")))
	{
		Appearance.Gender = EUEPalworldGender::TypeB;
		Appearance.BodyIndex = 2;
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("PalworldQATypeA")))
	{
		Appearance.Gender = EUEPalworldGender::TypeA;
		Appearance.BodyIndex = 1;
	}

	FString Value;
	if (FParse::Value(FCommandLine::Get(), TEXT("PalworldQAHead="), Value))
	{
		Appearance.HeadIndex = FCString::Atoi(*Value);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("PalworldQAHair="), Value))
	{
		Appearance.HairIndex = FCString::Atoi(*Value);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("PalworldQAEyes="), Value))
	{
		Appearance.EyeIndex = FCString::Atoi(*Value);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("PalworldQAOutfit="), Value))
	{
		Appearance.BodyEquipmentIndex = FCString::Atoi(*Value);
	}
}

void AUEPalworldCustomizationPreviewActor::CaptureQAWidgetScreenshot()
{
	// UI 구조 검증 때는 위젯을 지우지 않고 Slate/UMG까지 같이 캡처한다.
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		TEXT("Screenshots/Customization/Palworld_Widget_UI.png");
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	GetWorldTimerManager().SetTimer(QAExitTimer, this, &ThisClass::ExitAfterQAScreenshot, 1.5f, false);
}

void AUEPalworldCustomizationPreviewActor::CaptureQAScreenshot()
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

	auto LogPart = [](const TCHAR* Name, USkeletalMeshComponent* Component)
	{
		USkeletalMesh* Mesh = Component ? Component->GetSkeletalMeshAsset() : nullptr;
		UE_LOG(LogTemp, Display, TEXT("Palworld QA part %s mesh=%s visible=%d materials=%d bounds=%s"),
			Name,
			*GetPathNameSafe(Mesh),
			Component && Component->IsVisible(),
			Component ? Component->GetNumMaterials() : 0,
			Component ? *Component->Bounds.GetBox().ToString() : TEXT("none"));
	};

	LogPart(TEXT("BodyEquipment"), BodyEquipmentMesh);
	LogPart(TEXT("Head"), HeadMesh);
	LogPart(TEXT("Hair"), HairMesh);

	UE_LOG(LogTemp, Display,
		TEXT("Palworld QA selection gender=%s body=%d head=%d hair=%d eyes=%d outfit=%d"),
		Appearance.Gender == EUEPalworldGender::TypeA ? TEXT("TypeA") : TEXT("TypeB"),
		Appearance.BodyIndex,
		Appearance.HeadIndex,
		Appearance.HairIndex,
		Appearance.EyeIndex,
		Appearance.BodyEquipmentIndex);

	const TCHAR* GenderName = Appearance.Gender == EUEPalworldGender::TypeA ? TEXT("TypeA") : TEXT("TypeB");
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		FString::Printf(
			TEXT("Screenshots/Customization/Palworld_%s_H%02d_Hair%02d_Eye%02d_Outfit%02d_Full.png"),
			GenderName,
			Appearance.HeadIndex,
			Appearance.HairIndex,
			Appearance.EyeIndex,
			Appearance.BodyEquipmentIndex);
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	GetWorldTimerManager().SetTimer(QAPrepareHeadTimer, this, &ThisClass::PrepareQAHeadScreenshot, 1.0f, false);
}

void AUEPalworldCustomizationPreviewActor::PrepareQAHeadScreenshot()
{
	const FVector HeadCenter = HeadMesh
		? SceneRoot->GetComponentTransform().InverseTransformPosition(HeadMesh->Bounds.Origin)
		: FVector(0.0f, 0.0f, 150.0f);
	PreviewCamera->SetRelativeLocation(FVector(0.0f, 95.0f, HeadCenter.Z + 1.0f));
	PreviewCamera->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	PreviewCamera->FieldOfView = 32.0f;
	GetWorldTimerManager().SetTimer(QAHeadScreenshotTimer, this, &ThisClass::CaptureQAHeadScreenshot, 1.0f, false);
}

void AUEPalworldCustomizationPreviewActor::CaptureQAHeadScreenshot()
{
	const TCHAR* GenderName = Appearance.Gender == EUEPalworldGender::TypeA ? TEXT("TypeA") : TEXT("TypeB");
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		FString::Printf(
			TEXT("Screenshots/Customization/Palworld_%s_H%02d_Hair%02d_Eye%02d_Outfit%02d_Head.png"),
			GenderName,
			Appearance.HeadIndex,
			Appearance.HairIndex,
			Appearance.EyeIndex,
			Appearance.BodyEquipmentIndex);
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	GetWorldTimerManager().SetTimer(QAExitTimer, this, &ThisClass::ExitAfterQAScreenshot, 1.5f, false);
}

void AUEPalworldCustomizationPreviewActor::ExitAfterQAScreenshot()
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

void AUEPalworldCustomizationPreviewActor::ConfigurePreviewLighting()
{
	if (PreviewCamera)
	{
		PreviewCamera->SetRelativeLocation(FVector(0.0f, 230.0f, 125.0f));
		PreviewCamera->SetRelativeRotation(FRotator(-2.0f, -90.0f, 0.0f));
		PreviewCamera->FieldOfView = 50.0f;
		PreviewCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
		PreviewCamera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		PreviewCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
		PreviewCamera->PostProcessSettings.AutoExposureBias = 3.2f;
	}

	if (KeyLight)
	{
		KeyLight->SetRelativeLocation(FVector(0.0f, 140.0f, 155.0f));
		KeyLight->SetRelativeRotation(FRotator(-5.0f, -90.0f, 0.0f));
		KeyLight->SetIntensity(42000.0f);
		KeyLight->SetAttenuationRadius(1200.0f);
		KeyLight->SetOuterConeAngle(90.0f);
		KeyLight->SetInnerConeAngle(70.0f);
		KeyLight->SetCastShadows(false);
		KeyLight->SetLightColor(FLinearColor::White);
	}

	if (FillLight)
	{
		FillLight->SetIntensity(9.0f);
		FillLight->SetLightColor(FLinearColor::White);
	}

	if (FrontLight)
	{
		FrontLight->SetRelativeLocation(FVector(0.0f, 90.0f, 130.0f));
		FrontLight->SetIntensity(30000.0f);
		FrontLight->SetAttenuationRadius(650.0f);
		FrontLight->SetCastShadows(false);
		FrontLight->SetLightColor(FLinearColor::White);
	}
}

void AUEPalworldCustomizationPreviewActor::NormalizeLegacyDefaultColors()
{
	// 예전 프리뷰 BP가 저장한 검정 틴트는 원본 머티리얼을 덮어버린다.
	// 아주 어두운 기본값은 흰색으로 되돌려 Palworld 원본 텍스처를 그대로 보이게 한다.
	auto Normalize = [](FLinearColor& Color)
	{
		if (Color.R < 0.01f && Color.G < 0.01f && Color.B < 0.01f)
		{
			Color = FLinearColor::White;
		}
	};

	Normalize(Appearance.SkinColor);
	Normalize(Appearance.HairColor);
	Normalize(Appearance.EyeColor);
}

void AUEPalworldCustomizationPreviewActor::RefreshFollowerPose()
{
	USkeletalMesh* LeaderMesh = BodyEquipmentMesh ? BodyEquipmentMesh->GetSkeletalMeshAsset() : nullptr;
	const USkeleton* LeaderSkeleton = LeaderMesh ? LeaderMesh->GetSkeleton() : nullptr;

	for (USkeletalMeshComponent* Follower : {HeadMesh.Get(), HairMesh.Get()})
	{
		if (!Follower)
		{
			continue;
		}

		USkeletalMesh* FollowerMesh = Follower->GetSkeletalMeshAsset();
		if (FollowerMesh && (FollowerMesh->GetSkeleton() == LeaderSkeleton || HasMatchingReferenceSkeleton(LeaderMesh, FollowerMesh)))
		{
			Follower->SetLeaderPoseComponent(BodyEquipmentMesh, true, false);
		}
		else
		{
			Follower->SetLeaderPoseComponent(nullptr);
		}
		Follower->SetRelativeTransform(FTransform::Identity);
		Follower->SetVisibility(FollowerMesh != nullptr, true);
	}
}

void AUEPalworldCustomizationPreviewActor::ApplyMaterialColors()
{
	// 원본 의상 색은 건드리지 않고, 캐릭터 색상 항목만 선택적으로 틴트한다.
	if (!Appearance.SkinColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyColorToSlots(HeadMesh, Appearance.SkinColor, {TEXT("Head"), TEXT("Body"), TEXT("Skin")});
		ApplyColorToSlots(BodyEquipmentMesh, Appearance.SkinColor, {TEXT("Body"), TEXT("Skin")});
	}
	if (!Appearance.HairColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyColorToSlots(HairMesh, Appearance.HairColor, {TEXT("Hair")});
	}
	if (!Appearance.EyeColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyColorToSlots(HeadMesh, Appearance.EyeColor, {TEXT("Eye")});
	}
}

void AUEPalworldCustomizationPreviewActor::ApplyEyeMaterial(const FUEPalworldCustomizationOption& Option)
{
	if (!HeadMesh || !Option.Material)
	{
		return;
	}

	const TArray<FName> SlotNames = HeadMesh->GetMaterialSlotNames();
	for (int32 Index = 0; Index < SlotNames.Num(); ++Index)
	{
		if (SlotNames[Index].ToString().Contains(TEXT("Eye")))
		{
			HeadMesh->SetMaterial(Index, Option.Material);
		}
	}
}

void AUEPalworldCustomizationPreviewActor::ApplyColorToSlots(
	USkeletalMeshComponent* Component,
	const FLinearColor& Color,
	const TArray<FString>& SlotContains)
{
	if (!Component || Color.Equals(FLinearColor::White, 0.003f))
	{
		return;
	}

	const int32 MaterialCount = Component->GetNumMaterials();
	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		const FName SlotName = Component->GetMaterialSlotNames().IsValidIndex(Index)
			? Component->GetMaterialSlotNames()[Index]
			: NAME_None;
		bool bMatches = SlotContains.Num() == 0;
		for (const FString& Token : SlotContains)
		{
			if (SlotName.ToString().Contains(Token))
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
		if (!DynamicMaterial)
		{
			continue;
		}
		DynamicMaterial->SetVectorParameterValue(TEXT("TintColor"), Color);
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
	}
}

void AUEPalworldCustomizationPreviewActor::ResetComponentMaterials(USkeletalMeshComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// 저장된 MID override를 제거해서 선택한 메시의 원본 머티리얼 슬롯을 다시 쓰게 한다.
	const int32 MaterialCount = Component->GetNumMaterials();
	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		Component->SetMaterial(Index, nullptr);
	}
}

void AUEPalworldCustomizationPreviewActor::HideFaceCoverSections(USkeletalMeshComponent* Component)
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
		const bool bIsFaceCover =
			Slot.Contains(TEXT("mask")) ||
			Slot.Contains(TEXT("mouthcover")) ||
			Slot.Contains(TEXT("headcover")) ||
			Slot.Contains(TEXT("headequ")) ||
			Slot.Contains(TEXT("head_equip")) ||
			MaterialPath.Contains(TEXT("mask")) ||
			MaterialPath.Contains(TEXT("mouthcover")) ||
			MaterialPath.Contains(TEXT("headcover"));

		if (!bIsFaceCover)
		{
			continue;
		}

		// 얼굴을 가리는 장비 섹션은 커스터마이징 프리뷰에서 제외한다.
		for (int32 LODIndex = 0; LODIndex < 8; ++LODIndex)
		{
			Component->ShowMaterialSection(MaterialIndex, MaterialIndex, false, LODIndex);
		}
	}
}

void AUEPalworldCustomizationPreviewActor::ApplyScale()
{
	Appearance.HeightScale = FMath::Clamp(Appearance.HeightScale, 0.75f, 1.25f);
	Appearance.TorsoVolume = FMath::Clamp(Appearance.TorsoVolume, -1.0f, 1.0f);
	Appearance.ArmVolume = FMath::Clamp(Appearance.ArmVolume, -1.0f, 1.0f);
	Appearance.LegVolume = FMath::Clamp(Appearance.LegVolume, -1.0f, 1.0f);

	// Palworld식 체형 값은 얼굴이나 머리만 따로 떼어 줄이지 않는다.
	// 분리된 부품이 애니메이션과 같이 움직이도록 공통 루트만 조절한다.
	const float WidthScale = 1.0f + Appearance.TorsoVolume * 0.08f;
	const float HeightScale = 1.0f + Appearance.LegVolume * 0.05f;
	CharacterRoot->SetRelativeScale3D(FVector(WidthScale, WidthScale, HeightScale));

	for (USkeletalMeshComponent* Component : {BodyEquipmentMesh.Get(), HeadMesh.Get(), HairMesh.Get()})
	{
		if (Component)
		{
			Component->SetRelativeScale3D(FVector::OneVector);
			Component->SetRelativeLocation(FVector::ZeroVector);
		}
	}
}

const FUEPalworldCustomizationOption& AUEPalworldCustomizationPreviewActor::GetOption(
	EUEPalworldCustomizationCategory Category,
	int32 Index) const
{
	return Catalog ? Catalog->GetOption(Category, Index) : PreviewEmptyOption;
}

int32& AUEPalworldCustomizationPreviewActor::MutableIndex(EUEPalworldCustomizationCategory Category)
{
	switch (Category)
	{
	case EUEPalworldCustomizationCategory::Body:
		return Appearance.BodyIndex;
	case EUEPalworldCustomizationCategory::Head:
		return Appearance.HeadIndex;
	case EUEPalworldCustomizationCategory::Hair:
		return Appearance.HairIndex;
	case EUEPalworldCustomizationCategory::Eyes:
		return Appearance.EyeIndex;
	case EUEPalworldCustomizationCategory::BodyEquipment:
		return Appearance.BodyEquipmentIndex;
	default:
		return Appearance.BodyIndex;
	}
}

int32 AUEPalworldCustomizationPreviewActor::GetIndex(EUEPalworldCustomizationCategory Category) const
{
	switch (Category)
	{
	case EUEPalworldCustomizationCategory::Body:
		return Appearance.BodyIndex;
	case EUEPalworldCustomizationCategory::Head:
		return Appearance.HeadIndex;
	case EUEPalworldCustomizationCategory::Hair:
		return Appearance.HairIndex;
	case EUEPalworldCustomizationCategory::Eyes:
		return Appearance.EyeIndex;
	case EUEPalworldCustomizationCategory::BodyEquipment:
		return Appearance.BodyEquipmentIndex;
	default:
		return 0;
	}
}

int32 AUEPalworldCustomizationPreviewActor::ClampIndex(int32 Index, int32 Count)
{
	if (Count <= 0)
	{
		return 0;
	}
	return FMath::Clamp(Index, 0, Count - 1);
}
