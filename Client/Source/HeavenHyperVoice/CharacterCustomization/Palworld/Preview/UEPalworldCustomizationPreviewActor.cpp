#include "UEPalworldCustomizationPreviewActor.h"

#include "../Data/UEPalworldCustomizationTypes.h"

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
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"

namespace
{
	const FUEPalworldCustomizationOption PreviewEmptyOption;
	constexpr int32 PreviewMaxVisibleOutfits = 14;
	const TCHAR* MorphSafeMaterialFolder = TEXT("/Game/CharacterCustomization/Palworld/Generated/MorphSafeMaterials");
	const TCHAR* EyeCompositeFolder = TEXT("/Game/CharacterCustomization/Palworld/Generated/EyeComposite");

	USkeletalMeshComponent* CreateSkeletalPart(AActor* Owner, USceneComponent* Parent, const FName& Name)
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

	void ApplyPreviewSignedMorphTarget(USkeletalMeshComponent* Component, const FName MinTarget, const FName MaxTarget, float Value)
	{
		if (!Component)
		{
			return;
		}

		const float ClampedValue = FMath::Clamp(Value, -1.0f, 1.0f);
		Component->SetMorphTarget(MinTarget, ClampedValue < 0.0f ? -ClampedValue : 0.0f);
		Component->SetMorphTarget(MaxTarget, ClampedValue > 0.0f ? ClampedValue : 0.0f);
	}

	void SetPreviewMaterialShownOnAllLods(USkeletalMeshComponent* Component, int32 MaterialIndex, bool bShow)
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

	FString MakeMorphSafeMaterialName(const UMaterialInterface* Material)
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

	int32 ExtractPreviewPalworldEyeNumber(const FUEPalworldCustomizationOption& Option)
	{
		const FString Identity = FString::Printf(
			TEXT("%s %s %s"),
			*Option.Id,
			*Option.DisplayName,
			*GetPathNameSafe(Option.Material));
		const int32 EyeMarker = Identity.Find(TEXT("Eye"), ESearchCase::IgnoreCase);
		const int32 TypeMarker = Identity.Find(TEXT("Type"), ESearchCase::IgnoreCase);
		int32 Start = EyeMarker != INDEX_NONE ? EyeMarker + 3 : (TypeMarker != INDEX_NONE ? TypeMarker + 4 : 0);
		while (Start < Identity.Len() && !FChar::IsDigit(Identity[Start]))
		{
			++Start;
		}

		FString Digits;
		for (int32 Index = Start; Index < Identity.Len() && FChar::IsDigit(Identity[Index]); ++Index)
		{
			Digits.AppendChar(Identity[Index]);
		}
		return Digits.IsEmpty() ? 1 : FMath::Clamp(FCString::Atoi(*Digits), 1, 999);
	}

	int32 FindNearestPaletteIndex(const TArray<FLinearColor>& Palette, const FLinearColor& Color)
	{
		if (Palette.IsEmpty())
		{
			return 0;
		}

		int32 BestIndex = 0;
		float BestDistance = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Palette.Num(); ++Index)
		{
			const FLinearColor Candidate = Palette[Index].GetClamped();
			const float Distance =
				FMath::Square(Candidate.R - Color.R) +
				FMath::Square(Candidate.G - Color.G) +
				FMath::Square(Candidate.B - Color.B);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestIndex = Index;
			}
		}
		return FMath::Clamp(BestIndex, 0, 9);
	}

	UTexture* LoadEyeCompositeTexture(
		const FUEPalworldCustomizationOption& Option,
		const FLinearColor& EyeColor,
		const TArray<FLinearColor>& EyePalette)
	{
		const int32 EyeNumber = ExtractPreviewPalworldEyeNumber(Option);
		const int32 ColorIndex = FindNearestPaletteIndex(EyePalette, EyeColor.GetClamped());
		const FString TextureName = FString::Printf(
			TEXT("T_Player_Eye%03d_Composite_C%02d"),
			EyeNumber,
			ColorIndex);
		const FString TexturePath = FString::Printf(
			TEXT("%s/Eye%03d/%s.%s"),
			EyeCompositeFolder,
			EyeNumber,
			*TextureName,
			*TextureName);
		if (UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath))
		{
			return Texture;
		}

		const FString FallbackName = FString::Printf(TEXT("T_Player_Eye%03d_Composite"), EyeNumber);
		const FString FallbackPath = FString::Printf(
			TEXT("%s/Eye%03d/%s.%s"),
			EyeCompositeFolder,
			EyeNumber,
			*FallbackName,
			*FallbackName);
		return LoadObject<UTexture>(nullptr, *FallbackPath);
	}

	void ApplyPreviewEyeColorParameters(UMaterialInstanceDynamic* Material, const FLinearColor& EyeColor)
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

	void ApplyPreviewEyeTextureParameters(UMaterialInstanceDynamic* Material, UTexture* Texture)
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

}

AUEPalworldCustomizationPreviewActor::AUEPalworldCustomizationPreviewActor()
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

void AUEPalworldCustomizationPreviewActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshMeshes();
}

void AUEPalworldCustomizationPreviewActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	SetActorRotation(FRotator::ZeroRotator);
	SetActorScale3D(FVector::OneVector);
	PreviewYawDegrees = 0.0f;
	PreviewZoom = 1.0f;
	PreviewPanPixels = FVector2D::ZeroVector;
	if (CharacterRoot)
	{
		CharacterRoot->SetRelativeRotation(FRotator(0.0f, PreviewYawDegrees, 0.0f));
	}
	ConfigurePreviewLighting();
	ApplyQACommandLineAppearance();
	RefreshMeshes();
	PreparePreviewStage();

	if (FParse::Param(FCommandLine::Get(), TEXT("PalworldQAScreenshot")))
	{
		GetWorldTimerManager().SetTimer(QAScreenshotTimer, this, &ThisClass::CaptureQAScreenshot, 8.0f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("PalworldQAWidgetScreenshot")))
	{
		GetWorldTimerManager().SetTimer(QAWidgetScreenshotTimer, this, &ThisClass::CaptureQAWidgetScreenshot, 8.0f, false);
	}
}

void AUEPalworldCustomizationPreviewActor::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (PreviewCamera)
	{
		PreviewCamera->GetCameraView(DeltaTime, OutResult);
		return;
	}

	Super::CalcCamera(DeltaTime, OutResult);
}

void AUEPalworldCustomizationPreviewActor::AddPreviewYaw(float DeltaYaw)
{
	PreviewYawDegrees = FMath::UnwindDegrees(PreviewYawDegrees + DeltaYaw);
	if (CharacterRoot)
	{
		CharacterRoot->SetRelativeRotation(FRotator(0.0f, PreviewYawDegrees, 0.0f));
	}
}

void AUEPalworldCustomizationPreviewActor::AddPreviewZoom(float DeltaZoom)
{
	PreviewZoom = FMath::Clamp(PreviewZoom + DeltaZoom, 0.55f, 1.85f);
	FramePreviewCamera();
}

void AUEPalworldCustomizationPreviewActor::AddPreviewPan(FVector2D DeltaPixels)
{
	PreviewPanPixels.X = FMath::Clamp(PreviewPanPixels.X + DeltaPixels.X, -260.0f, 260.0f);
	PreviewPanPixels.Y = FMath::Clamp(PreviewPanPixels.Y + DeltaPixels.Y, -180.0f, 180.0f);
	FramePreviewCamera();
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
	// 의상 색은 원본 Palworld 머티리얼과 텍스처를 그대로 사용한다.
		break;
	}
	RefreshMeshes();
}

void AUEPalworldCustomizationPreviewActor::SetScaleValue(EUEPalworldScaleChannel Channel, float Value)
{
	NormalizeLegacyDefaultColors();
	const float Percent = FMath::Clamp(Value, 0.0f, 100.0f);
	const float VolumeValue = (Percent - 50.0f) / 50.0f;
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
	const int32 BodyEquipmentCount = GetOptionCount(EUEPalworldCustomizationCategory::BodyEquipment);
	if (BodyEquipmentCount > 1)
	{
		const int32 VisibleOutfitMaxIndex = FMath::Min(BodyEquipmentCount - 1, PreviewMaxVisibleOutfits);
		Appearance.BodyEquipmentIndex = FMath::Clamp(Appearance.BodyEquipmentIndex, 1, VisibleOutfitMaxIndex);
	}
	else
	{
		Appearance.BodyEquipmentIndex = ClampIndex(Appearance.BodyEquipmentIndex, BodyEquipmentCount);
	}
	// 얼굴을 가리는 장비 섹션은 숨기고 머리 장비 컴포넌트는 사용하지 않는다.
	const FUEPalworldCustomizationOption& Body = GetOption(
		EUEPalworldCustomizationCategory::Body,
		Appearance.BodyIndex);
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
	USkeletalMesh* BaseMesh = Body.LoadMesh(Appearance.Gender);
	USkeletalMesh* EquipmentMesh = BodyEquipment.LoadMesh(Appearance.Gender);
	const bool bSameOutfitAsBase =
		EquipmentMesh == BaseMesh || GetPathNameSafe(EquipmentMesh).Equals(GetPathNameSafe(BaseMesh));
	const bool bUsesSeparateOutfit = EquipmentMesh && !bSameOutfitAsBase && Appearance.BodyEquipmentIndex > 0;
	BaseBodyMesh->SetSkeletalMesh(BaseMesh);
	ResetComponentMaterials(BaseBodyMesh);
	// Palworld 원본 의상/피부 텍스처를 그대로 써야 하므로 커마 프리뷰에서는 대체 머티리얼을 덮지 않는다.
	HideFaceCoverSections(BaseBodyMesh);
	if (bUsesSeparateOutfit)
	{
		HideBaseBodyOutfitSections(BaseBodyMesh);
	}
	BodyEquipmentMesh->SetSkeletalMesh(bUsesSeparateOutfit ? EquipmentMesh : nullptr);
	BodyEquipmentMesh->SetVisibility(bUsesSeparateOutfit, true);
	BodyEquipmentMesh->SetHiddenInGame(!bUsesSeparateOutfit, true);
	ResetComponentMaterials(BodyEquipmentMesh);
	// 별도 의상도 원본 SkeletalMesh에 박힌 머티리얼 슬롯을 그대로 사용한다.
	HideFaceCoverSections(BodyEquipmentMesh);
	HeadMesh->SetSkeletalMesh(Head.LoadMesh(Appearance.Gender));
	ResetComponentMaterials(HeadMesh);
	HideFaceCoverSections(HeadMesh);
	HairMesh->SetSkeletalMesh(Hair.LoadMesh(Appearance.Gender));
	ResetComponentMaterials(HairMesh);

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
	if (FParse::Value(FCommandLine::Get(), TEXT("PalworldQATorso="), Value))
	{
		Appearance.TorsoVolume = FMath::Clamp(FCString::Atof(*Value), -1.0f, 1.0f);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("PalworldQAArm="), Value))
	{
		Appearance.ArmVolume = FMath::Clamp(FCString::Atof(*Value), -1.0f, 1.0f);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("PalworldQALeg="), Value))
	{
		Appearance.LegVolume = FMath::Clamp(FCString::Atof(*Value), -1.0f, 1.0f);
	}
}

void AUEPalworldCustomizationPreviewActor::CaptureQAWidgetScreenshot()
{
	// UI 구조 검증 때는 실제 화면을 캡처해서 UMG와 함께 확인한다.
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
			TEXT("Palworld QA part %s mesh=%s visible=%d materials=%d torsoMin=%.2f torsoMax=%.2f armMin=%.2f armMax=%.2f legMin=%.2f legMax=%.2f bounds=%s"),
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
		TEXT("Palworld QA selection gender=%s body=%d head=%d hair=%d eyes=%d outfit=%d torso=%.2f arm=%.2f leg=%.2f"),
		Appearance.Gender == EUEPalworldGender::TypeA ? TEXT("TypeA") : TEXT("TypeB"),
		Appearance.BodyIndex,
		Appearance.HeadIndex,
		Appearance.HairIndex,
		Appearance.EyeIndex,
		Appearance.BodyEquipmentIndex,
		Appearance.TorsoVolume,
		Appearance.ArmVolume,
		Appearance.LegVolume);

	const TCHAR* GenderName = Appearance.Gender == EUEPalworldGender::TypeA ? TEXT("TypeA") : TEXT("TypeB");
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		FString::Printf(
			TEXT("Screenshots/Customization/Palworld_%s_H%02d_Hair%02d_Eye%02d_Outfit%02d_T%+.1f_A%+.1f_L%+.1f_Full.png"),
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
			TEXT("Screenshots/Customization/Palworld_%s_H%02d_Hair%02d_Eye%02d_Outfit%02d_T%+.1f_A%+.1f_L%+.1f_Head.png"),
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

void AUEPalworldCustomizationPreviewActor::FramePreviewCamera()
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

void AUEPalworldCustomizationPreviewActor::PreparePreviewStage()
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

		Component->SetHiddenInGame(false, true);
		Component->SetVisibility(true, true);
	}
}

void AUEPalworldCustomizationPreviewActor::NormalizeLegacyDefaultColors()
{
	// 이전 프리뷰 BP가 저장한 검은/흰 MID override를 제거한다.
	// 아주 어두운 기본값이 단색으로 돌아가지 않게 Palworld 원본 텍스처를 그대로 보이게 한다.
	const FUEPalworldAppearance Defaults;
	if (Appearance.SkinColor.R < 0.01f && Appearance.SkinColor.G < 0.01f && Appearance.SkinColor.B < 0.01f)
	{
		Appearance.SkinColor = Defaults.SkinColor;
	}
	if (Appearance.HairColor.R < 0.01f && Appearance.HairColor.G < 0.01f && Appearance.HairColor.B < 0.01f)
	{
		Appearance.HairColor = Defaults.HairColor;
	}
	if (Appearance.EyeColor.R < 0.01f && Appearance.EyeColor.G < 0.01f && Appearance.EyeColor.B < 0.01f)
	{
		Appearance.EyeColor = Defaults.EyeColor;
	}
}

void AUEPalworldCustomizationPreviewActor::RefreshFollowerPose()
{
	for (USkeletalMeshComponent* Follower : {BodyEquipmentMesh.Get(), HeadMesh.Get(), HairMesh.Get()})
	{
		if (!Follower)
		{
			continue;
		}

		USkeletalMesh* FollowerMesh = Follower->GetSkeletalMeshAsset();
		const USkeleton* LeaderSkeleton = BaseBodyMesh && BaseBodyMesh->GetSkeletalMeshAsset()
			? BaseBodyMesh->GetSkeletalMeshAsset()->GetSkeleton()
			: nullptr;
		const USkeleton* FollowerSkeleton = FollowerMesh ? FollowerMesh->GetSkeleton() : nullptr;
		if (FollowerMesh && LeaderSkeleton && FollowerSkeleton == LeaderSkeleton)
		{
			// 같은 스켈레톤을 쓰는 부위만 LeaderPose로 묶는다.
			// 다른 스켈레톤 추출 파트를 억지로 묶으면 머리카락이 렌더링에서 사라질 수 있다.
			Follower->SetLeaderPoseComponent(BaseBodyMesh, true, false);
		}
		else
		{
			Follower->SetLeaderPoseComponent(nullptr);
		}
		Follower->SetRelativeTransform(FTransform::Identity);
		Follower->SetVisibility(FollowerMesh != nullptr, true);
	}
}

void AUEPalworldCustomizationPreviewActor::HideUnsupportedAttachmentComponents()
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

		// 이번 Palworld 커마에서는 부착물을 쓰지 않는다.
		// BP에 남은 이전 헤드기어/마스크 컴포넌트가 화면에 나오지 않게 막는다.
		Component->SetLeaderPoseComponent(nullptr);
		Component->SetSkeletalMesh(nullptr);
		Component->SetVisibility(false, true);
		Component->SetHiddenInGame(true, true);
	}
}

void AUEPalworldCustomizationPreviewActor::ApplyMaterialColors()
{
	// 원본 의상 색은 건드리지 않고, 캐릭터 색상 항목만 선택적으로 틴트한다.
	if (!Appearance.SkinColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyColorToSlots(HeadMesh, Appearance.SkinColor, {TEXT("Head"), TEXT("Body"), TEXT("Skin")});
		ApplyColorToSlots(BaseBodyMesh, Appearance.SkinColor, {TEXT("Body"), TEXT("Skin")});
		ApplyColorToSlots(BodyEquipmentMesh, Appearance.SkinColor, {TEXT("Body"), TEXT("Skin")});
	}
	if (!Appearance.HairColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyColorToSlots(HairMesh, Appearance.HairColor, {TEXT("Hair")});
	}
}

void AUEPalworldCustomizationPreviewActor::ApplyEyeMaterial(const FUEPalworldCustomizationOption& Option)
{
	if (!HeadMesh || !Option.Material)
	{
		return;
	}

	UTexture* CompositeTexture = LoadEyeCompositeTexture(
		Option,
		Appearance.EyeColor,
		Catalog ? Catalog->EyeColors : TArray<FLinearColor>());
	const int32 MaterialCount = HeadMesh->GetNumMaterials();
	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		if (IsEyeIrisMaterialSlot(HeadMesh, Index))
		{
			UMaterialInstanceDynamic* EyeMaterial = HeadMesh->CreateDynamicMaterialInstance(Index, Option.Material);
			if (!EyeMaterial)
			{
				HeadMesh->SetMaterial(Index, Option.Material);
				continue;
			}
			if (CompositeTexture)
			{
				// 흰자, 홍채, 동공, 하이라이트가 합쳐진 Palworld 눈 텍스처만 갈아 끼운다.
				ApplyPreviewEyeTextureParameters(EyeMaterial, CompositeTexture);
			}
			ApplyPreviewEyeColorParameters(EyeMaterial, Appearance.EyeColor);
		}
	}
}

bool AUEPalworldCustomizationPreviewActor::IsEyeIrisMaterialSlot(
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

	// 눈 머티리얼은 Palworld 눈 머티리얼 슬롯만 교체한다.
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
	const bool bHasPalworldEyeMaterial =
		MaterialIdentity.IsEmpty() ||
		MaterialIdentity.Contains(TEXT("mi_player_eye")) ||
		MaterialIdentity.Contains(TEXT("player_eye")) ||
		MaterialIdentity.Contains(TEXT("iris")) ||
		MaterialIdentity.Contains(TEXT("pupil"));
	return bLooksLikeEyeSlot && bHasPalworldEyeMaterial;
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
			DynamicMaterial->SetVectorParameterValue(TEXT("TintColor"), Color);
		}
	}
}

void AUEPalworldCustomizationPreviewActor::ResetComponentMaterials(USkeletalMeshComponent* Component)
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
	}
}

void AUEPalworldCustomizationPreviewActor::ApplyMorphSafeMaterials(USkeletalMeshComponent* Component)
{
	if (!Component)
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

		const FString SafePath = FString::Printf(TEXT("%s/%s.%s"), MorphSafeMaterialFolder, *SafeName, *SafeName);
		UMaterialInterface* SafeMaterial = LoadObject<UMaterialInterface>(nullptr, *SafePath);
		if (SafeMaterial)
		{
			// 원본 머티리얼을 부모로 둔 MorphSafe 인스턴스를 써서, 모프 적용 후 기본 회색 머티리얼로 떨어지는 일을 막는다.
			Component->SetMaterial(Index, SafeMaterial);
		}
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

void AUEPalworldCustomizationPreviewActor::HideBaseBodyOutfitSections(USkeletalMeshComponent* Component)
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

		// 베이스 바디가 OldCloth 메시라서 다른 의상을 입을 때 기본 옷 섹션이 겹칠 수 있다.
		// 별도 의상이 켜져 있으면 베이스의 옷 섹션만 숨기고 피부 섹션은 유지한다.
		SetPreviewMaterialShownOnAllLods(Component, MaterialIndex, false);
	}
}

void AUEPalworldCustomizationPreviewActor::ApplyScale()
{
	Appearance.TorsoVolume = FMath::Clamp(Appearance.TorsoVolume, -1.0f, 1.0f);
	Appearance.ArmVolume = FMath::Clamp(Appearance.ArmVolume, -1.0f, 1.0f);
	Appearance.LegVolume = FMath::Clamp(Appearance.LegVolume, -1.0f, 1.0f);

	// Palworld 체형 값은 얼굴이나 머리만 따로 줄이지 않는다.
	// 루트와 부위 스케일은 항상 1로 두고, 원본 체형 모프만 적용한다.
	// 슬라이더의 0~100 값은 -1~+1 모프 값으로 바꿔 몸통/팔/다리에만 반영한다.
	CharacterRoot->SetRelativeScale3D(FVector::OneVector);

	for (USkeletalMeshComponent* Component : {BaseBodyMesh.Get(), BodyEquipmentMesh.Get(), HeadMesh.Get(), HairMesh.Get()})
	{
		if (Component)
		{
			Component->SetRelativeScale3D(FVector::OneVector);
			Component->SetRelativeLocation(FVector::ZeroVector);
		}
	}

	// Palworld 원본 체형 모프만 사용한다. 머리와 루트 스케일은 따로 건드리지 않는다.
	ApplyPreviewSignedMorphTarget(BaseBodyMesh, TEXT("BS_Torso_min"), TEXT("BS_Torso_max"), Appearance.TorsoVolume);
	ApplyPreviewSignedMorphTarget(BaseBodyMesh, TEXT("BS_Arm_min"), TEXT("BS_Arm_max"), Appearance.ArmVolume);
	ApplyPreviewSignedMorphTarget(BaseBodyMesh, TEXT("BS_Leg_min"), TEXT("BS_Leg_max"), Appearance.LegVolume);
	ApplyPreviewSignedMorphTarget(BodyEquipmentMesh, TEXT("BS_Torso_min"), TEXT("BS_Torso_max"), Appearance.TorsoVolume);
	ApplyPreviewSignedMorphTarget(BodyEquipmentMesh, TEXT("BS_Arm_min"), TEXT("BS_Arm_max"), Appearance.ArmVolume);
	ApplyPreviewSignedMorphTarget(BodyEquipmentMesh, TEXT("BS_Leg_min"), TEXT("BS_Leg_max"), Appearance.LegVolume);
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
