#include "UECustomizationPreviewActor.h"

#include "../Data/UECustomizationCatalog.h"
#include "../Data/UECharacterCustomizationSaveGame.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialParameters.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

const FString AUECustomizationPreviewActor::SaveSlotName(TEXT("UECharacterAppearance"));

namespace
{
	const FName DiffuseColorParameter(TEXT("DiffuseColor"));
	const FLinearColor DefaultBodySkinColor(0.937254902f, 0.725490196f, 0.592156863f, 1.0f);
	const FLinearColor DefaultEyeIrisColor(0.138431615f, 0.323143209f, 0.445201195f, 1.0f);
	const TCHAR* WhiteTexturePath = TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture");
	const TCHAR* FemaleBodyNormalTexturePath = TEXT("/Game/VRoidGenerated/Female/BodySkin_SubmeshKey_N00_000_00_Body_00_SKIN_female_body_normal.BodySkin_SubmeshKey_N00_000_00_Body_00_SKIN_female_body_normal");

	bool IsIdentityTint(const FLinearColor& Color)
	{
		return Color.Equals(FLinearColor::White, 0.003f);
	}

	bool IsBrokenHairFrontStyle(int32 Index)
	{
		// Style 2393's extracted front mesh contains flat horizontal strips across the face.
		return Index == 5;
	}

	bool ReadTextureParameter(UMaterialInterface* SourceMaterial, const FName& ParameterName, UTexture*& OutTexture)
	{
		OutTexture = nullptr;
		return SourceMaterial &&
			SourceMaterial->GetTextureParameterValue(FHashedMaterialParameterInfo(ParameterName), OutTexture, true) &&
			OutTexture;
	}

	bool ReadVectorParameter(UMaterialInterface* SourceMaterial, const FName& ParameterName, FLinearColor& OutColor)
	{
		OutColor = FLinearColor::White;
		return SourceMaterial &&
			SourceMaterial->GetVectorParameterValue(FHashedMaterialParameterInfo(ParameterName), OutColor, true);
	}

	bool CopyFirstTextureParameter(
		UMaterialInterface* SourceMaterial,
		UMaterialInstanceDynamic* TargetMaterial,
		const TArrayView<const FName> SourceParameterNames,
		const FName& TargetParameterName,
		UTexture*& OutTexture)
	{
		for (const FName& SourceParameterName : SourceParameterNames)
		{
			if (ReadTextureParameter(SourceMaterial, SourceParameterName, OutTexture))
			{
				TargetMaterial->SetTextureParameterValue(TargetParameterName, OutTexture);
				return true;
			}
		}
		OutTexture = nullptr;
		return false;
	}

	bool CopyFirstVectorParameter(
		UMaterialInterface* SourceMaterial,
		UMaterialInstanceDynamic* TargetMaterial,
		const TArrayView<const FName> SourceParameterNames,
		const FName& TargetParameterName,
		FLinearColor& OutColor)
	{
		for (const FName& SourceParameterName : SourceParameterNames)
		{
			if (ReadVectorParameter(SourceMaterial, SourceParameterName, OutColor))
			{
				OutColor.A = 1.0f;
				TargetMaterial->SetVectorParameterValue(TargetParameterName, OutColor);
				return true;
			}
		}
		OutColor = FLinearColor::White;
		return false;
	}

	void PreserveSourceMaterialTextures(UMaterialInterface* SourceMaterial, UMaterialInstanceDynamic* TargetMaterial)
	{
		if (!SourceMaterial || !TargetMaterial)
		{
			return;
		}
		static const TCHAR* DefaultNormalPath = TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal");

		static const FName DiffuseParameterNames[] = {
			TEXT("DiffuseColorMap"),
			TEXT("MainImage"),
			TEXT("BaseColorMap"),
			TEXT("BaseMap"),
			TEXT("_MainTex")};
		static const FName OpacityParameterNames[] = {
			TEXT("OpacityMaskMap"),
			TEXT("AlphaMap"),
			TEXT("MaskMap"),
			TEXT("_AlphaTex")};
		static const FName NormalParameterNames[] = {
			TEXT("NormalMap"),
			TEXT("NormalMapImage"),
			TEXT("BumpMap"),
			TEXT("_BumpMap")};
		static const FName SourceTintParameterNames[] = {
			TEXT("DiffuseColor"),
			TEXT("BaseColor"),
			TEXT("Base Color"),
			TEXT("Color"),
			TEXT("TintColor")};

		FLinearColor SourceTint = FLinearColor::White;
		const bool bHasSourceTint = CopyFirstVectorParameter(
			SourceMaterial, TargetMaterial, SourceTintParameterNames, TEXT("OriginalTint"), SourceTint);
		if (!bHasSourceTint)
		{
			TargetMaterial->SetVectorParameterValue(TEXT("OriginalTint"), FLinearColor::White);
		}
		TargetMaterial->SetVectorParameterValue(TEXT("CustomTint"), FLinearColor::White);

		UTexture* DiffuseTexture = nullptr;
		const bool bHasDiffuseTexture = CopyFirstTextureParameter(
			SourceMaterial, TargetMaterial, DiffuseParameterNames, TEXT("DiffuseColorMap"), DiffuseTexture);
		const FLinearColor RuntimeBaseColor = bHasDiffuseTexture ? FLinearColor::White : SourceTint;
		TargetMaterial->SetVectorParameterValue(TEXT("DiffuseColor"), RuntimeBaseColor);
		TargetMaterial->SetVectorParameterValue(TEXT("BaseColor"), RuntimeBaseColor);
		TargetMaterial->SetVectorParameterValue(TEXT("Base Color"), RuntimeBaseColor);
		TargetMaterial->SetVectorParameterValue(TEXT("TintColor"), RuntimeBaseColor);
		TargetMaterial->SetVectorParameterValue(TEXT("Color"), RuntimeBaseColor);

		if (bHasDiffuseTexture)
		{
			TargetMaterial->SetTextureParameterValue(TEXT("EmissiveColorMap"), DiffuseTexture);
			TargetMaterial->SetScalarParameterValue(TEXT("DiffuseColorMapWeight"), 1.0f);
		}
		else if (UTexture* WhiteTexture = LoadObject<UTexture>(nullptr, WhiteTexturePath))
		{
			DiffuseTexture = WhiteTexture;
			TargetMaterial->SetTextureParameterValue(TEXT("DiffuseColorMap"), WhiteTexture);
			TargetMaterial->SetTextureParameterValue(TEXT("EmissiveColorMap"), WhiteTexture);
			TargetMaterial->SetScalarParameterValue(TEXT("DiffuseColorMapWeight"), 0.0f);
		}

		UTexture* OpacityTexture = nullptr;
		const bool bHasOpacityTexture = CopyFirstTextureParameter(
			SourceMaterial, TargetMaterial, OpacityParameterNames, TEXT("OpacityMaskMap"), OpacityTexture);
		if (!bHasOpacityTexture &&
			DiffuseTexture)
		{
			TargetMaterial->SetTextureParameterValue(TEXT("OpacityMaskMap"), DiffuseTexture);
		}
		TargetMaterial->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
		TargetMaterial->SetScalarParameterValue(TEXT("OpacityMask"), 1.0f);
		TargetMaterial->SetScalarParameterValue(TEXT("OpacityMaskMapWeight"), (bHasDiffuseTexture || bHasOpacityTexture) ? 1.0f : 0.0f);

		UTexture* NormalTexture = nullptr;
		if (CopyFirstTextureParameter(SourceMaterial, TargetMaterial, NormalParameterNames, TEXT("NormalMap"), NormalTexture))
		{
			TargetMaterial->SetScalarParameterValue(TEXT("NormalMapWeight"), 1.0f);
		}
		else if (UTexture* DefaultNormal = LoadObject<UTexture>(nullptr, DefaultNormalPath))
		{
			TargetMaterial->SetTextureParameterValue(TEXT("NormalMap"), DefaultNormal);
			TargetMaterial->SetScalarParameterValue(TEXT("NormalMapWeight"), 0.0f);
		}
		else
		{
			TargetMaterial->SetScalarParameterValue(TEXT("NormalMapWeight"), 0.0f);
		}
	}

	void PreserveRuntimeTextureWeights(const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials)
	{
		for (UMaterialInstanceDynamic* Material : Materials)
		{
			if (!Material)
			{
				continue;
			}

			const float DiffuseWeight = Material->K2_GetScalarParameterValue(TEXT("DiffuseColorMapWeight"));
			const float OpacityWeight = Material->K2_GetScalarParameterValue(TEXT("OpacityMaskMapWeight"));
			Material->SetScalarParameterValue(TEXT("DiffuseColorMapWeight"), DiffuseWeight > 0.5f ? 1.0f : 0.0f);
			Material->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
			Material->SetScalarParameterValue(TEXT("OpacityMask"), 1.0f);
			Material->SetScalarParameterValue(TEXT("OpacityMaskMapWeight"), OpacityWeight > 0.5f ? 1.0f : 0.0f);
		}
	}

	UTexture2D* FindPreviewTextureFromMesh(const USkeletalMesh* Mesh)
	{
		if (!Mesh)
		{
			return nullptr;
		}

		static const FName DiffuseParameterNames[] = {
			TEXT("DiffuseColorMap"),
			TEXT("MainImage"),
			TEXT("BaseColorMap"),
			TEXT("BaseMap"),
			TEXT("_MainTex")};
		for (const FSkeletalMaterial& Slot : Mesh->GetMaterials())
		{
			UMaterialInterface* Material = Slot.MaterialInterface;
			if (!Material)
			{
				continue;
			}
			for (const FName& ParameterName : DiffuseParameterNames)
			{
				UTexture* Texture = nullptr;
				if (ReadTextureParameter(Material, ParameterName, Texture))
				{
					if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
					{
						return Texture2D;
					}
				}
			}
		}
		return nullptr;
	}

	FString BuildMeshShapeKey(const USkeletalMesh* Mesh)
	{
		if (!Mesh)
		{
			return TEXT("None");
		}

		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const FVector Origin = Bounds.Origin;
		const FVector Extent = Bounds.BoxExtent;
		const UTexture2D* PreviewTexture = FindPreviewTextureFromMesh(Mesh);
		return FString::Printf(
			TEXT("O=%.2f,%.2f,%.2f|E=%.2f,%.2f,%.2f|R=%.2f|Slots=%d|Tex=%s"),
			Origin.X, Origin.Y, Origin.Z,
			Extent.X, Extent.Y, Extent.Z,
			Bounds.SphereRadius,
			Mesh->GetMaterials().Num(),
			PreviewTexture ? *PreviewTexture->GetName() : TEXT("None"));
	}

	bool ShouldUseShapeKeyForOptions(EUECustomizationPart Part)
	{
		return Part == EUECustomizationPart::HairBase ||
			Part == EUECustomizationPart::HairFront ||
			Part == EUECustomizationPart::HairSide ||
			Part == EUECustomizationPart::HairBack ||
			Part == EUECustomizationPart::HairExtra;
	}

	void ScaleComponentAroundPivot(USkeletalMeshComponent* Component, const FVector& Pivot, float Scale)
	{
		if (!Component)
		{
			return;
		}
		Component->SetRelativeScale3D(FVector(Scale));
		Component->SetRelativeLocation(Pivot - Pivot * Scale);
	}

	UTexture2D* LoadHairThumbnail(EUECustomizationPart Part, bool bFemale, int32 RawIndex)
	{
		if (Part != EUECustomizationPart::HairSet &&
			!ShouldUseShapeKeyForOptions(Part))
		{
			return nullptr;
		}
		if (Part == EUECustomizationPart::HairExtra && RawIndex == 0)
		{
			return nullptr;
		}

		const TCHAR* GenderName = bFemale ? TEXT("Female") : TEXT("Male");
		const FString AssetName = FString::Printf(TEXT("T_%s_Hair_%d"), GenderName, RawIndex);
		const FString AssetPath = FString::Printf(
			TEXT("/Game/CharacterCustomization/UI/HairThumbnails/%s.%s"),
			*AssetName,
			*AssetName);
		return LoadObject<UTexture2D>(nullptr, *AssetPath);
	}
}

AUECustomizationPreviewActor::AUECustomizationPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CharacterRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CharacterRoot"));
	CharacterRoot->SetupAttachment(SceneRoot);

	PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
	PreviewCamera->SetupAttachment(SceneRoot);
	PreviewCamera->SetRelativeLocation(FVector(720.0f, 0.0f, 92.0f));
	PreviewCamera->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	PreviewCamera->FieldOfView = 32.0f;
	PreviewCamera->bAutoActivate = true;
	PreviewCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	PreviewCamera->PostProcessSettings.MotionBlurAmount = 0.0f;

	PortraitLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("VRoidPortraitLight"));
	PortraitLight->SetupAttachment(SceneRoot);
	PortraitLight->SetRelativeLocation(FVector(420.0f, 0.0f, 105.0f));
	PortraitLight->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	PortraitLight->SetUseInverseSquaredFalloff(false);
	PortraitLight->SetLightFalloffExponent(2.0f);
	PortraitLight->SetIntensity(24.0f);
	PortraitLight->SetAttenuationRadius(1000.0f);
	PortraitLight->SetInnerConeAngle(20.0f);
	PortraitLight->SetOuterConeAngle(42.0f);
	PortraitLight->SetCastShadows(false);
	PortraitLight->LightingChannels.bChannel0 = false;
	PortraitLight->LightingChannels.bChannel1 = true;

	BodyMesh = CreatePart(TEXT("BodySkin"));
	FaceSkinMesh = CreatePart(TEXT("FaceSkin"));
	EyeWhiteMesh = CreatePart(TEXT("EyeWhite"));
	EyeIrisMesh = CreatePart(TEXT("EyeIris"));
	EyeHighlightMesh = CreatePart(TEXT("EyeHighlight"));
	EyeExtraMesh = CreatePart(TEXT("EyeExtra"));
	BrowMesh = CreatePart(TEXT("Brow"));
	EyelashMesh = CreatePart(TEXT("Eyelash"));
	EyelineMesh = CreatePart(TEXT("Eyeline"));
	MouthMesh = CreatePart(TEXT("Mouth"));
	LipOverlayMesh = CreatePart(TEXT("LipOverlay"));
	MouthLineOverlayMesh = CreatePart(TEXT("MouthLineOverlay"));
	HairScalpMesh = CreatePart(TEXT("HairScalp"));
	HairBaseMesh = CreatePart(TEXT("HairBase"));
	HairFrontMesh = CreatePart(TEXT("HairFront"));
	HairSideMesh = CreatePart(TEXT("HairSide"));
	HairBackMesh = CreatePart(TEXT("HairBack"));
	HairExtraMesh = CreatePart(TEXT("HairExtra"));
	HairScalpMesh->SetCastShadow(false);
	HairBaseMesh->SetCastShadow(false);
	HairFrontMesh->SetCastShadow(false);
	HairSideMesh->SetCastShadow(false);
	HairBackMesh->SetCastShadow(false);
	HairExtraMesh->SetCastShadow(false);
	TopMesh = CreatePart(TEXT("Top"));
	BottomMesh = CreatePart(TEXT("Bottom"));
	OnepieceMesh = CreatePart(TEXT("Onepiece"));
	ShoesMesh = CreatePart(TEXT("Shoes"));
	HeadAccessoryMesh = CreatePart(TEXT("HeadAccessory"));
	FaceAccessoryMesh = CreatePart(TEXT("FaceAccessory"));
	EarAccessoryMesh = CreatePart(TEXT("EarAccessory"));
	TailAccessoryMesh = CreatePart(TEXT("TailAccessory"));
	NeckAccessoryMesh = CreatePart(TEXT("NeckAccessory"));
}

USkeletalMeshComponent* AUECustomizationPreviewActor::CreatePart(const FName& PartName)
{
	USkeletalMeshComponent* Part = CreateDefaultSubobject<USkeletalMeshComponent>(PartName);
	Part->SetupAttachment(CharacterRoot);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCastShadow(false);
	Part->bUseAttachParentBound = false;
	Part->LightingChannels.bChannel0 = false;
	Part->LightingChannels.bChannel1 = true;
	return Part;
}

USkeletalMesh* AUECustomizationPreviewActor::SelectMesh(const TArray<TObjectPtr<USkeletalMesh>>& Catalog, int32 Index) const
{
	return Catalog.IsEmpty() ? nullptr : Catalog[FMath::Clamp(Index, 0, Catalog.Num() - 1)].Get();
}

const TArray<TObjectPtr<USkeletalMesh>>& AUECustomizationPreviewActor::GetCatalog(EUECustomizationPart Part) const
{
	const bool bFemale = Appearance.Gender == EUECharacterGender::Female;
#define UE_SELECT_CATALOG(Name) (bFemale ? Female##Name##Catalog : Male##Name##Catalog)
	switch (Part)
	{
	case EUECustomizationPart::Body: return UE_SELECT_CATALOG(Body);
	case EUECustomizationPart::FaceSkin: return UE_SELECT_CATALOG(FaceSkin);
	case EUECustomizationPart::EyeWhite: return UE_SELECT_CATALOG(EyeWhite);
	case EUECustomizationPart::EyeIris: return UE_SELECT_CATALOG(EyeIris);
	case EUECustomizationPart::EyeHighlight: return UE_SELECT_CATALOG(EyeHighlight);
	case EUECustomizationPart::EyeExtra: return UE_SELECT_CATALOG(EyeExtra);
	case EUECustomizationPart::Brow: return UE_SELECT_CATALOG(Brow);
	case EUECustomizationPart::Eyelash: return UE_SELECT_CATALOG(Eyelash);
	case EUECustomizationPart::Eyeline: return UE_SELECT_CATALOG(Eyeline);
	case EUECustomizationPart::Mouth: return UE_SELECT_CATALOG(Mouth);
	case EUECustomizationPart::HairBase: return UE_SELECT_CATALOG(HairBase);
	case EUECustomizationPart::HairFront: return UE_SELECT_CATALOG(HairFront);
	case EUECustomizationPart::HairSide: return UE_SELECT_CATALOG(HairSide);
	case EUECustomizationPart::HairBack: return UE_SELECT_CATALOG(HairBack);
	case EUECustomizationPart::HairExtra: return UE_SELECT_CATALOG(HairExtra);
	case EUECustomizationPart::Top: return UE_SELECT_CATALOG(Top);
	case EUECustomizationPart::Bottom: return UE_SELECT_CATALOG(Bottom);
	case EUECustomizationPart::Onepiece: return UE_SELECT_CATALOG(Onepiece);
	case EUECustomizationPart::Shoes: return UE_SELECT_CATALOG(Shoes);
	case EUECustomizationPart::HeadAccessory: return UE_SELECT_CATALOG(HeadAccessory);
	case EUECustomizationPart::FaceAccessory: return UE_SELECT_CATALOG(FaceAccessory);
	case EUECustomizationPart::EarAccessory: return UE_SELECT_CATALOG(EarAccessory);
	case EUECustomizationPart::TailAccessory: return UE_SELECT_CATALOG(TailAccessory);
	case EUECustomizationPart::NeckAccessory: return UE_SELECT_CATALOG(NeckAccessory);
	default:
	{
		static const TArray<TObjectPtr<USkeletalMesh>> EmptyCatalog;
		return EmptyCatalog;
	}
	}
#undef UE_SELECT_CATALOG
}

const TArray<TObjectPtr<UTexture2D>>& AUECustomizationPreviewActor::GetTextureCatalog(EUECustomizationPart Part) const
{
	switch (Part)
	{
	case EUECustomizationPart::FaceSkin: return FaceSkinTextureCatalog;
	case EUECustomizationPart::EyeWhite: return EyeWhiteTextureCatalog;
	case EUECustomizationPart::EyeIris: return EyeIrisTextureCatalog;
	case EUECustomizationPart::EyeHighlight: return EyeHighlightTextureCatalog;
	case EUECustomizationPart::Brow: return BrowTextureCatalog;
	case EUECustomizationPart::Eyelash: return EyelashTextureCatalog;
	case EUECustomizationPart::Eyeline: return EyelineTextureCatalog;
	case EUECustomizationPart::Mouth: return MouthTextureCatalog;
	case EUECustomizationPart::Lip: return LipTextureCatalog;
	case EUECustomizationPart::MouthLine: return MouthLineTextureCatalog;
	default:
	{
		static const TArray<TObjectPtr<UTexture2D>> Empty;
		return Empty;
	}
	}
}

void AUECustomizationPreviewActor::InitializeCatalogs()
{
	ApplyCatalogAsset();
}

void AUECustomizationPreviewActor::ApplyCatalogAsset()
{
	if (!CatalogAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("BP_CustomizationPreviewActor requires a Customization Catalog asset."));
		return;
	}

#define UE_COPY_MESH_CATALOG(Name) Name = CatalogAsset->Name
	UE_COPY_MESH_CATALOG(MaleBodyCatalog);
	UE_COPY_MESH_CATALOG(MaleFaceSkinCatalog);
	UE_COPY_MESH_CATALOG(MaleEyeWhiteCatalog);
	UE_COPY_MESH_CATALOG(MaleEyeIrisCatalog);
	UE_COPY_MESH_CATALOG(MaleEyeHighlightCatalog);
	UE_COPY_MESH_CATALOG(MaleEyeExtraCatalog);
	UE_COPY_MESH_CATALOG(MaleBrowCatalog);
	UE_COPY_MESH_CATALOG(MaleEyelashCatalog);
	UE_COPY_MESH_CATALOG(MaleEyelineCatalog);
	UE_COPY_MESH_CATALOG(MaleMouthCatalog);
	UE_COPY_MESH_CATALOG(MaleHairBaseCatalog);
	UE_COPY_MESH_CATALOG(MaleHairFrontCatalog);
	UE_COPY_MESH_CATALOG(MaleHairSideCatalog);
	UE_COPY_MESH_CATALOG(MaleHairBackCatalog);
	UE_COPY_MESH_CATALOG(MaleHairExtraCatalog);
	UE_COPY_MESH_CATALOG(MaleTopCatalog);
	UE_COPY_MESH_CATALOG(MaleBottomCatalog);
	UE_COPY_MESH_CATALOG(MaleOnepieceCatalog);
	UE_COPY_MESH_CATALOG(MaleShoesCatalog);
	UE_COPY_MESH_CATALOG(MaleHeadAccessoryCatalog);
	UE_COPY_MESH_CATALOG(MaleFaceAccessoryCatalog);
	UE_COPY_MESH_CATALOG(MaleEarAccessoryCatalog);
	UE_COPY_MESH_CATALOG(MaleTailAccessoryCatalog);
	UE_COPY_MESH_CATALOG(MaleNeckAccessoryCatalog);
	UE_COPY_MESH_CATALOG(FemaleBodyCatalog);
	UE_COPY_MESH_CATALOG(FemaleFaceSkinCatalog);
	UE_COPY_MESH_CATALOG(FemaleEyeWhiteCatalog);
	UE_COPY_MESH_CATALOG(FemaleEyeIrisCatalog);
	UE_COPY_MESH_CATALOG(FemaleEyeHighlightCatalog);
	UE_COPY_MESH_CATALOG(FemaleEyeExtraCatalog);
	UE_COPY_MESH_CATALOG(FemaleBrowCatalog);
	UE_COPY_MESH_CATALOG(FemaleEyelashCatalog);
	UE_COPY_MESH_CATALOG(FemaleEyelineCatalog);
	UE_COPY_MESH_CATALOG(FemaleMouthCatalog);
	UE_COPY_MESH_CATALOG(FemaleHairBaseCatalog);
	UE_COPY_MESH_CATALOG(FemaleHairFrontCatalog);
	UE_COPY_MESH_CATALOG(FemaleHairSideCatalog);
	UE_COPY_MESH_CATALOG(FemaleHairBackCatalog);
	UE_COPY_MESH_CATALOG(FemaleHairExtraCatalog);
	UE_COPY_MESH_CATALOG(FemaleTopCatalog);
	UE_COPY_MESH_CATALOG(FemaleBottomCatalog);
	UE_COPY_MESH_CATALOG(FemaleOnepieceCatalog);
	UE_COPY_MESH_CATALOG(FemaleShoesCatalog);
	UE_COPY_MESH_CATALOG(FemaleHeadAccessoryCatalog);
	UE_COPY_MESH_CATALOG(FemaleFaceAccessoryCatalog);
	UE_COPY_MESH_CATALOG(FemaleEarAccessoryCatalog);
	UE_COPY_MESH_CATALOG(FemaleTailAccessoryCatalog);
	UE_COPY_MESH_CATALOG(FemaleNeckAccessoryCatalog);
#undef UE_COPY_MESH_CATALOG

#define UE_COPY_TEXTURE_CATALOG(Name) Name = CatalogAsset->Name
	UE_COPY_TEXTURE_CATALOG(FaceSkinTextureCatalog);
	UE_COPY_TEXTURE_CATALOG(FaceSkinNormalTextureCatalog);
	UE_COPY_TEXTURE_CATALOG(EyeWhiteTextureCatalog);
	UE_COPY_TEXTURE_CATALOG(EyeIrisTextureCatalog);
	UE_COPY_TEXTURE_CATALOG(EyeHighlightTextureCatalog);
	UE_COPY_TEXTURE_CATALOG(BrowTextureCatalog);
	UE_COPY_TEXTURE_CATALOG(EyelashTextureCatalog);
	UE_COPY_TEXTURE_CATALOG(EyelineTextureCatalog);
	UE_COPY_TEXTURE_CATALOG(MouthTextureCatalog);
	UE_COPY_TEXTURE_CATALOG(LipTextureCatalog);
	UE_COPY_TEXTURE_CATALOG(MouthLineTextureCatalog);
#undef UE_COPY_TEXTURE_CATALOG
	ValidateCatalogSkeletons();
}

void AUECustomizationPreviewActor::ValidateCatalogSkeletons() const
{
	if (!CatalogAsset->CommonSkeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("VRoid customization catalog has no CommonSkeleton assigned."));
		return;
	}

	int32 Checked = 0;
	int32 Mismatches = 0;
	auto CheckCatalog = [&Checked, &Mismatches, this](const TCHAR* CatalogName, const TArray<TObjectPtr<USkeletalMesh>>& Meshes)
	{
		for (USkeletalMesh* Mesh : Meshes)
		{
			if (!Mesh)
			{
				continue;
			}
			++Checked;
			if (Mesh->GetSkeleton() != CatalogAsset->CommonSkeleton)
			{
				++Mismatches;
				UE_LOG(LogTemp, Error, TEXT("VRoid mesh %s in %s uses a different Skeleton (%s)."),
					*Mesh->GetPathName(), CatalogName, Mesh->GetSkeleton() ? *Mesh->GetSkeleton()->GetPathName() : TEXT("None"));
			}
		}
	};

#define UE_CHECK_MESH_CATALOG(Name) CheckCatalog(TEXT(#Name), Name)
	UE_CHECK_MESH_CATALOG(MaleBodyCatalog);
	UE_CHECK_MESH_CATALOG(MaleFaceSkinCatalog);
	UE_CHECK_MESH_CATALOG(MaleEyeWhiteCatalog);
	UE_CHECK_MESH_CATALOG(MaleEyeIrisCatalog);
	UE_CHECK_MESH_CATALOG(MaleEyeHighlightCatalog);
	UE_CHECK_MESH_CATALOG(MaleEyeExtraCatalog);
	UE_CHECK_MESH_CATALOG(MaleBrowCatalog);
	UE_CHECK_MESH_CATALOG(MaleEyelashCatalog);
	UE_CHECK_MESH_CATALOG(MaleEyelineCatalog);
	UE_CHECK_MESH_CATALOG(MaleMouthCatalog);
	UE_CHECK_MESH_CATALOG(MaleHairBaseCatalog);
	UE_CHECK_MESH_CATALOG(MaleHairFrontCatalog);
	UE_CHECK_MESH_CATALOG(MaleHairSideCatalog);
	UE_CHECK_MESH_CATALOG(MaleHairBackCatalog);
	UE_CHECK_MESH_CATALOG(MaleHairExtraCatalog);
	UE_CHECK_MESH_CATALOG(MaleTopCatalog);
	UE_CHECK_MESH_CATALOG(MaleBottomCatalog);
	UE_CHECK_MESH_CATALOG(MaleOnepieceCatalog);
	UE_CHECK_MESH_CATALOG(MaleShoesCatalog);
	UE_CHECK_MESH_CATALOG(MaleHeadAccessoryCatalog);
	UE_CHECK_MESH_CATALOG(MaleFaceAccessoryCatalog);
	UE_CHECK_MESH_CATALOG(MaleEarAccessoryCatalog);
	UE_CHECK_MESH_CATALOG(MaleTailAccessoryCatalog);
	UE_CHECK_MESH_CATALOG(MaleNeckAccessoryCatalog);
	UE_CHECK_MESH_CATALOG(FemaleBodyCatalog);
	UE_CHECK_MESH_CATALOG(FemaleFaceSkinCatalog);
	UE_CHECK_MESH_CATALOG(FemaleEyeWhiteCatalog);
	UE_CHECK_MESH_CATALOG(FemaleEyeIrisCatalog);
	UE_CHECK_MESH_CATALOG(FemaleEyeHighlightCatalog);
	UE_CHECK_MESH_CATALOG(FemaleEyeExtraCatalog);
	UE_CHECK_MESH_CATALOG(FemaleBrowCatalog);
	UE_CHECK_MESH_CATALOG(FemaleEyelashCatalog);
	UE_CHECK_MESH_CATALOG(FemaleEyelineCatalog);
	UE_CHECK_MESH_CATALOG(FemaleMouthCatalog);
	UE_CHECK_MESH_CATALOG(FemaleHairBaseCatalog);
	UE_CHECK_MESH_CATALOG(FemaleHairFrontCatalog);
	UE_CHECK_MESH_CATALOG(FemaleHairSideCatalog);
	UE_CHECK_MESH_CATALOG(FemaleHairBackCatalog);
	UE_CHECK_MESH_CATALOG(FemaleHairExtraCatalog);
	UE_CHECK_MESH_CATALOG(FemaleTopCatalog);
	UE_CHECK_MESH_CATALOG(FemaleBottomCatalog);
	UE_CHECK_MESH_CATALOG(FemaleOnepieceCatalog);
	UE_CHECK_MESH_CATALOG(FemaleShoesCatalog);
	UE_CHECK_MESH_CATALOG(FemaleHeadAccessoryCatalog);
	UE_CHECK_MESH_CATALOG(FemaleFaceAccessoryCatalog);
	UE_CHECK_MESH_CATALOG(FemaleEarAccessoryCatalog);
	UE_CHECK_MESH_CATALOG(FemaleTailAccessoryCatalog);
	UE_CHECK_MESH_CATALOG(FemaleNeckAccessoryCatalog);
#undef UE_CHECK_MESH_CATALOG

	if (Mismatches == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("VRoid customization skeleton validation passed: %d mesh entries use %s."),
			Checked, *CatalogAsset->CommonSkeleton->GetPathName());
	}
}

void AUECustomizationPreviewActor::ClampAppearanceToCatalogs()
{
	auto ClampMeshIndex = [this](EUECustomizationPart Part, int32& Index)
	{
		const int32 Count = GetCatalog(Part).Num();
		Index = Count > 0 ? FMath::Clamp(Index, 0, Count - 1) : 0;
	};
	auto ClampOptionIndex = [this](EUECustomizationPart Part, int32& Index)
	{
		const TArray<TObjectPtr<UTexture2D>>& Textures = GetTextureCatalog(Part);
		const int32 Count = Textures.IsEmpty() ? GetCatalog(Part).Num() : Textures.Num();
		Index = Count > 0 ? FMath::Clamp(Index, 0, Count - 1) : 0;
	};

	ClampOptionIndex(EUECustomizationPart::FaceSkin, Appearance.FaceStyle);
	ClampOptionIndex(EUECustomizationPart::EyeWhite, Appearance.EyeWhiteStyle);
	ClampOptionIndex(EUECustomizationPart::EyeIris, Appearance.EyeIrisStyle);
	ClampOptionIndex(EUECustomizationPart::EyeHighlight, Appearance.EyeHighlightStyle);
	ClampMeshIndex(EUECustomizationPart::EyeExtra, Appearance.EyeExtraStyle);
	ClampOptionIndex(EUECustomizationPart::Brow, Appearance.BrowStyle);
	ClampOptionIndex(EUECustomizationPart::Eyelash, Appearance.EyelashStyle);
	ClampOptionIndex(EUECustomizationPart::Eyeline, Appearance.EyelineStyle);
	ClampOptionIndex(EUECustomizationPart::Mouth, Appearance.MouthStyle);
	ClampOptionIndex(EUECustomizationPart::Lip, Appearance.LipStyle);
	ClampOptionIndex(EUECustomizationPart::MouthLine, Appearance.MouthLineStyle);
	ClampMeshIndex(EUECustomizationPart::HairBase, Appearance.HairBaseStyle);
	ClampMeshIndex(EUECustomizationPart::HairFront, Appearance.HairFrontStyle);
	ClampMeshIndex(EUECustomizationPart::HairSide, Appearance.HairSideStyle);
	ClampMeshIndex(EUECustomizationPart::HairBack, Appearance.HairBackStyle);
	ClampMeshIndex(EUECustomizationPart::HairExtra, Appearance.HairExtraStyle);
	ClampMeshIndex(EUECustomizationPart::Top, Appearance.TopStyle);
	ClampMeshIndex(EUECustomizationPart::Bottom, Appearance.BottomStyle);
	ClampMeshIndex(EUECustomizationPart::Onepiece, Appearance.OnepieceStyle);
	ClampMeshIndex(EUECustomizationPart::Shoes, Appearance.ShoesStyle);
	ClampMeshIndex(EUECustomizationPart::HeadAccessory, Appearance.HeadAccessoryStyle);
	ClampMeshIndex(EUECustomizationPart::FaceAccessory, Appearance.FaceAccessoryStyle);
	ClampMeshIndex(EUECustomizationPart::EarAccessory, Appearance.EarAccessoryStyle);
	ClampMeshIndex(EUECustomizationPart::TailAccessory, Appearance.TailAccessoryStyle);
	ClampMeshIndex(EUECustomizationPart::NeckAccessory, Appearance.NeckAccessoryStyle);

	if (Appearance.OnepieceStyle > 0)
	{
		Appearance.TopStyle = 0;
		Appearance.BottomStyle = 0;
	}
}

int32 AUECustomizationPreviewActor::GetOptionCount(EUECustomizationPart Part) const
{
	TArray<int32> RawIndices;
	BuildOptionIndexMap(Part, RawIndices);
	return RawIndices.Num();
}

int32 AUECustomizationPreviewActor::ResolveOptionIndex(EUECustomizationPart Part, int32 DisplayIndex) const
{
	TArray<int32> RawIndices;
	BuildOptionIndexMap(Part, RawIndices);
	return RawIndices.IsValidIndex(DisplayIndex)
		? RawIndices[DisplayIndex]
		: FMath::Max(DisplayIndex, 0);
}

int32 AUECustomizationPreviewActor::GetDisplayIndex(EUECustomizationPart Part, int32 RawIndex) const
{
	TArray<int32> RawIndices;
	BuildOptionIndexMap(Part, RawIndices);
	for (int32 DisplayIndex = 0; DisplayIndex < RawIndices.Num(); ++DisplayIndex)
	{
		if (RawIndices[DisplayIndex] == RawIndex)
		{
			return DisplayIndex;
		}
	}
	return RawIndices.IsEmpty() ? 0 : FMath::Clamp(RawIndex, 0, RawIndices.Num() - 1);
}

void AUECustomizationPreviewActor::BuildOptionIndexMap(EUECustomizationPart Part, TArray<int32>& OutRawIndices) const
{
	OutRawIndices.Reset();
	if (Part == EUECustomizationPart::Gender)
	{
		OutRawIndices.Add(0);
		OutRawIndices.Add(1);
		return;
	}
	if (Part == EUECustomizationPart::Body)
	{
		OutRawIndices.Add(0);
		OutRawIndices.Add(1);
		OutRawIndices.Add(2);
		return;
	}

	TSet<FString> SeenKeys;
	auto AddUnique = [&OutRawIndices, &SeenKeys](int32 RawIndex, FString Key)
	{
		if (Key.IsEmpty())
		{
			Key = FString::Printf(TEXT("Raw:%d"), RawIndex);
		}
		if (SeenKeys.Contains(Key))
		{
			return;
		}
		SeenKeys.Add(Key);
		OutRawIndices.Add(RawIndex);
	};

	if (Part == EUECustomizationPart::HairSet)
	{
		const int32 Count = FMath::Min(
			GetCatalog(EUECustomizationPart::HairFront).Num(),
			FMath::Min(GetCatalog(EUECustomizationPart::HairBack).Num(), GetCatalog(EUECustomizationPart::HairSide).Num()));
		for (int32 RawIndex = 0; RawIndex < Count; ++RawIndex)
		{
			FString Key;
			for (EUECustomizationPart HairPart : {
				EUECustomizationPart::HairFront,
				EUECustomizationPart::HairSide,
				EUECustomizationPart::HairBack,
				EUECustomizationPart::HairExtra})
			{
				const TArray<TObjectPtr<USkeletalMesh>>& Catalog = GetCatalog(HairPart);
				USkeletalMesh* Mesh = Catalog.IsValidIndex(RawIndex) ? Catalog[RawIndex].Get() : nullptr;
				Key += BuildMeshShapeKey(Mesh);
				Key += TEXT("|");
			}
			AddUnique(RawIndex, Key);
		}
		return;
	}

	const TArray<TObjectPtr<UTexture2D>>& Textures = GetTextureCatalog(Part);
	if (!Textures.IsEmpty())
	{
		for (int32 RawIndex = 0; RawIndex < Textures.Num(); ++RawIndex)
		{
			UTexture2D* Texture = Textures[RawIndex].Get();
			AddUnique(RawIndex, Texture ? Texture->GetPathName() : TEXT("None"));
		}
		return;
	}

	const TArray<TObjectPtr<USkeletalMesh>>& Meshes = GetCatalog(Part);
	for (int32 RawIndex = 0; RawIndex < Meshes.Num(); ++RawIndex)
	{
		USkeletalMesh* Mesh = Meshes[RawIndex].Get();
		AddUnique(RawIndex, ShouldUseShapeKeyForOptions(Part)
			? BuildMeshShapeKey(Mesh)
			: (Mesh ? Mesh->GetPathName() : TEXT("None")));
	}
}

FString AUECustomizationPreviewActor::GetOptionLabel(EUECustomizationPart Part, int32 Index) const
{
	const int32 RawIndex = ResolveOptionIndex(Part, Index);
	if (Part == EUECustomizationPart::Gender)
	{
		return RawIndex == 0 ? TEXT("Male") : TEXT("Female");
	}
	if (Part == EUECustomizationPart::Body)
	{
		static const TCHAR* Names[] = {TEXT("Slim"), TEXT("Standard"), TEXT("Wide")};
		return Names[FMath::Clamp(RawIndex, 0, 2)];
	}
	if (Part == EUECustomizationPart::HairSet ||
		Part == EUECustomizationPart::HairFront ||
		Part == EUECustomizationPart::HairSide ||
		Part == EUECustomizationPart::HairBack ||
		Part == EUECustomizationPart::HairExtra)
	{
		static const TArray<int32> EmptyStyleIds;
		const TArray<int32>& StyleIds = !CatalogAsset
			? EmptyStyleIds
			: Appearance.Gender == EUECharacterGender::Female
				? CatalogAsset->FemaleHairStyleIds
				: CatalogAsset->MaleHairStyleIds;
		if (StyleIds.IsValidIndex(RawIndex))
		{
			FString HairDescriptor;
			if (Part == EUECustomizationPart::HairSet)
			{
				const TArray<TObjectPtr<USkeletalMesh>>& BackCatalog = GetCatalog(EUECustomizationPart::HairBack);
				const TArray<TObjectPtr<USkeletalMesh>>& SideCatalog = GetCatalog(EUECustomizationPart::HairSide);
				const TArray<TObjectPtr<USkeletalMesh>>& ExtraCatalog = GetCatalog(EUECustomizationPart::HairExtra);
				USkeletalMesh* BackMesh = BackCatalog.IsValidIndex(RawIndex) ? BackCatalog[RawIndex].Get() : nullptr;
				USkeletalMesh* SideMesh = SideCatalog.IsValidIndex(RawIndex) ? SideCatalog[RawIndex].Get() : nullptr;
				USkeletalMesh* ExtraMesh = ExtraCatalog.IsValidIndex(RawIndex) ? ExtraCatalog[RawIndex].Get() : nullptr;
				if (BackMesh && BackMesh->GetBounds().BoxExtent.Z > 25.0f)
				{
					HairDescriptor = TEXT("Long Back");
				}
				else if (SideMesh && SideMesh->GetBounds().BoxExtent.X > 12.0f)
				{
					HairDescriptor = TEXT("Side Volume");
				}
				else
				{
					HairDescriptor = TEXT("Short");
				}
				if (ExtraMesh)
				{
					HairDescriptor += TEXT(" + Extra");
				}
			}
			return Part == EUECustomizationPart::HairSet
				? FString::Printf(TEXT("Hair Set %d\n%s"), StyleIds[RawIndex], *HairDescriptor)
				: FString::Printf(TEXT("Hair %d"), StyleIds[RawIndex]);
		}
		return FString::Printf(TEXT("Hair %02d"), RawIndex + 1);
	}
	if (UTexture2D* Texture = GetOptionTexture(Part, Index))
	{
		if (Texture->GetPathName().StartsWith(TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/")))
		{
			return TEXT("VRoid Default");
		}
		FString StyleId = Texture->GetName();
		StyleId.Split(TEXT("_"), nullptr, &StyleId, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		const bool bNonePreset =
			(Part == EUECustomizationPart::EyeIris && StyleId == TEXT("100")) ||
			(Part == EUECustomizationPart::EyeHighlight && StyleId == TEXT("000")) ||
			(Part == EUECustomizationPart::Brow && StyleId == TEXT("100")) ||
			(Part == EUECustomizationPart::Eyeline && StyleId == TEXT("100"));
		if (bNonePreset)
		{
			return TEXT("None");
		}
		return FString::Printf(TEXT("Preset %s"), *StyleId);
	}
	if (!GetTextureCatalog(Part).IsEmpty() && GetTextureCatalog(Part).IsValidIndex(RawIndex) &&
		GetTextureCatalog(Part)[RawIndex] == nullptr)
	{
		return TEXT("None");
	}
	if (Part == EUECustomizationPart::EyeExtra && RawIndex == 0)
	{
		return TEXT("None");
	}
	if (Part == EUECustomizationPart::HairBase)
	{
		static const TCHAR* Names[] = {TEXT("None"), TEXT("Default"), TEXT("Preset 001"), TEXT("Initial")};
		return Names[FMath::Clamp(RawIndex, 0, 3)];
	}
	const TArray<TObjectPtr<USkeletalMesh>>& MeshCatalog = GetCatalog(Part);
	if (MeshCatalog.IsValidIndex(RawIndex) && MeshCatalog[RawIndex] == nullptr)
	{
		return TEXT("None");
	}
	if (MeshCatalog.IsValidIndex(RawIndex) && MeshCatalog[RawIndex])
	{
		FString Label = MeshCatalog[RawIndex]->GetName();
		Label.RemoveFromStart(TEXT("SK_"));
		Label.ReplaceInline(TEXT("_Male_"), TEXT(" "));
		Label.ReplaceInline(TEXT("_Female_"), TEXT(" "));
		Label.ReplaceInline(TEXT("_"), TEXT(" "));
		return Label;
	}
	return FString::Printf(TEXT("Style %02d"), RawIndex + 1);
}

UTexture2D* AUECustomizationPreviewActor::GetOptionTexture(EUECustomizationPart Part, int32 Index) const
{
	const int32 RawIndex = ResolveOptionIndex(Part, Index);
	if (Part == EUECustomizationPart::HairSet || ShouldUseShapeKeyForOptions(Part))
	{
		return LoadHairThumbnail(Part, Appearance.Gender == EUECharacterGender::Female, RawIndex);
	}
	const TArray<TObjectPtr<UTexture2D>>& Catalog = GetTextureCatalog(Part);
	if (Catalog.IsValidIndex(RawIndex))
	{
		return Catalog[RawIndex].Get();
	}

	const TArray<TObjectPtr<USkeletalMesh>>& MeshCatalog = GetCatalog(Part);
	return MeshCatalog.IsValidIndex(RawIndex) ? FindPreviewTextureFromMesh(MeshCatalog[RawIndex].Get()) : nullptr;
}

bool AUECustomizationPreviewActor::UpdateMeshes()
{
	Appearance.Normalize();
	ClampAppearanceToCatalogs();
	bool bChanged = false;
	auto SelectTexturedMesh = [this](EUECustomizationPart Part, int32 Index) -> USkeletalMesh*
	{
		const TArray<TObjectPtr<USkeletalMesh>>& Catalog = GetCatalog(Part);
		if (Catalog.IsEmpty())
		{
			return nullptr;
		}
		if (Catalog.IsValidIndex(Index) && Catalog[Index])
		{
			return Catalog[Index].Get();
		}
		for (const TObjectPtr<USkeletalMesh>& Mesh : Catalog)
		{
			if (Mesh)
			{
				return Mesh.Get();
			}
		}
		return nullptr;
	};
	auto Assign = [&bChanged](USkeletalMeshComponent* Component, USkeletalMesh* Mesh)
	{
		if (Component->GetSkeletalMeshAsset() != Mesh)
		{
			Component->SetSkeletalMesh(Mesh);
			bChanged = true;
		}
		Component->SetVisibility(Mesh != nullptr, true);
	};
	auto SelectOptionalMesh = [this](EUECustomizationPart Part, int32 Index) -> USkeletalMesh*
	{
		if (Index <= 0)
		{
			return nullptr;
		}
		const TArray<TObjectPtr<USkeletalMesh>>& Catalog = GetCatalog(Part);
		return Catalog.IsValidIndex(Index) ? Catalog[Index].Get() : nullptr;
	};

	Assign(BodyMesh, SelectMesh(GetCatalog(EUECustomizationPart::Body), 0));
	Assign(FaceSkinMesh, SelectTexturedMesh(EUECustomizationPart::FaceSkin, Appearance.FaceStyle));
	Assign(EyeWhiteMesh, SelectTexturedMesh(EUECustomizationPart::EyeWhite, Appearance.EyeWhiteStyle));
	Assign(EyeIrisMesh, SelectTexturedMesh(EUECustomizationPart::EyeIris, Appearance.EyeIrisStyle));
	Assign(EyeHighlightMesh, SelectTexturedMesh(EUECustomizationPart::EyeHighlight, Appearance.EyeHighlightStyle));
	Assign(EyeExtraMesh, SelectMesh(GetCatalog(EUECustomizationPart::EyeExtra), Appearance.EyeExtraStyle));
	Assign(BrowMesh, SelectTexturedMesh(EUECustomizationPart::Brow, Appearance.BrowStyle));
	Assign(EyelashMesh, SelectTexturedMesh(EUECustomizationPart::Eyelash, Appearance.EyelashStyle));
	Assign(EyelineMesh, SelectTexturedMesh(EUECustomizationPart::Eyeline, Appearance.EyelineStyle));
	Assign(MouthMesh, SelectTexturedMesh(EUECustomizationPart::Mouth, Appearance.MouthStyle));
	Assign(LipOverlayMesh, SelectTexturedMesh(EUECustomizationPart::FaceSkin, Appearance.FaceStyle));
	Assign(MouthLineOverlayMesh, SelectTexturedMesh(EUECustomizationPart::FaceSkin, Appearance.FaceStyle));
	USkeletalMesh* SelectedHairBase = SelectOptionalMesh(EUECustomizationPart::HairBase, Appearance.HairBaseStyle);
	const int32 VisibleHairFrontStyle = IsBrokenHairFrontStyle(Appearance.HairFrontStyle)
		? 0
		: Appearance.HairFrontStyle;
	Assign(HairScalpMesh, SelectedHairBase);
	Assign(HairBaseMesh, SelectedHairBase);
	Assign(HairFrontMesh, SelectMesh(GetCatalog(EUECustomizationPart::HairFront), VisibleHairFrontStyle));
	Assign(HairSideMesh, SelectMesh(GetCatalog(EUECustomizationPart::HairSide), Appearance.HairSideStyle));
	Assign(HairBackMesh, SelectMesh(GetCatalog(EUECustomizationPart::HairBack), Appearance.HairBackStyle));
	Assign(HairExtraMesh, SelectMesh(GetCatalog(EUECustomizationPart::HairExtra), Appearance.HairExtraStyle));
	Assign(TopMesh, SelectOptionalMesh(EUECustomizationPart::Top, Appearance.TopStyle));
	Assign(BottomMesh, SelectOptionalMesh(EUECustomizationPart::Bottom, Appearance.BottomStyle));
	Assign(OnepieceMesh, SelectOptionalMesh(EUECustomizationPart::Onepiece, Appearance.OnepieceStyle));
	Assign(ShoesMesh, SelectOptionalMesh(EUECustomizationPart::Shoes, Appearance.ShoesStyle));
	Assign(HeadAccessoryMesh, SelectOptionalMesh(EUECustomizationPart::HeadAccessory, Appearance.HeadAccessoryStyle));
	Assign(FaceAccessoryMesh, SelectOptionalMesh(EUECustomizationPart::FaceAccessory, Appearance.FaceAccessoryStyle));
	Assign(EarAccessoryMesh, SelectOptionalMesh(EUECustomizationPart::EarAccessory, Appearance.EarAccessoryStyle));
	Assign(TailAccessoryMesh, SelectOptionalMesh(EUECustomizationPart::TailAccessory, Appearance.TailAccessoryStyle));
	Assign(NeckAccessoryMesh, SelectOptionalMesh(EUECustomizationPart::NeckAccessory, Appearance.NeckAccessoryStyle));

	if (bChanged)
	{
		BindFollowerPoses();
	}
	return bChanged;
}

void AUECustomizationPreviewActor::BindFollowerPoses()
{
	const TArray<USkeletalMeshComponent*> Followers = {
		FaceSkinMesh, EyeWhiteMesh, EyeIrisMesh, EyeHighlightMesh, EyeExtraMesh, BrowMesh, EyelashMesh,
		EyelineMesh, MouthMesh, LipOverlayMesh, MouthLineOverlayMesh,
		HairScalpMesh, HairBaseMesh, HairFrontMesh, HairSideMesh, HairBackMesh, HairExtraMesh,
		TopMesh, BottomMesh, OnepieceMesh, ShoesMesh,
		HeadAccessoryMesh, FaceAccessoryMesh, EarAccessoryMesh, TailAccessoryMesh, NeckAccessoryMesh};
	for (USkeletalMeshComponent* Follower : Followers)
	{
		if (!Follower)
		{
			continue;
		}
		if (Follower->GetAttachParent() != CharacterRoot)
		{
			Follower->AttachToComponent(CharacterRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
		Follower->SetRelativeLocation(FVector::ZeroVector);
		Follower->SetRelativeRotation(FRotator::ZeroRotator);
		Follower->SetRelativeScale3D(FVector::OneVector);
		Follower->SetLeaderPoseComponent(BodyMesh, true, false);
	}
}

void AUECustomizationPreviewActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	InitializeCatalogs();
	Appearance.Normalize();
	ClampAppearanceToCatalogs();
	PreviewCamera->SetRelativeLocation(FVector(720.0f, 0.0f, 92.0f));
	PreviewCamera->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	CharacterRoot->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	HideLegacyPreviewParts();
	UpdateMeshes();
	ApplyTransforms();
}

void AUECustomizationPreviewActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeCatalogs();
	PreviewCamera->SetRelativeLocation(FVector(720.0f, 0.0f, 92.0f));
	PreviewCamera->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	CharacterRoot->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	HideLegacyPreviewParts();
	LoadAppearance();
	if (IsIdentityTint(Appearance.EyeColor))
	{
		Appearance.EyeColor = DefaultEyeIrisColor;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("VRoidQAReset")))
	{
		Appearance = FUECharacterCustomizationData();
		Appearance.EyeColor = DefaultEyeIrisColor;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("VRoidQAFemale")))
	{
		Appearance.Gender = EUECharacterGender::Female;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("VRoidQASecondStyle")))
	{
		Appearance.FaceStyle = 1;
		Appearance.EyeWhiteStyle = 1;
		Appearance.EyeIrisStyle = 1;
		Appearance.EyeHighlightStyle = 1;
		Appearance.EyeExtraStyle = 1;
		Appearance.BrowStyle = 1;
		Appearance.EyelashStyle = 1;
		Appearance.EyelineStyle = 1;
		Appearance.MouthStyle = 1;
		Appearance.LipStyle = 1;
		Appearance.MouthLineStyle = 1;
		Appearance.HairBaseStyle = 1;
		Appearance.HairFrontStyle = 1;
		Appearance.HairSideStyle = 1;
		Appearance.HairBackStyle = 1;
	}
	int32 QAFaceIndex = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("VRoidQAFaceIndex="), QAFaceIndex))
	{
		Appearance.FaceStyle = QAFaceIndex;
		Appearance.EyeWhiteStyle = QAFaceIndex;
		Appearance.EyeIrisStyle = QAFaceIndex;
		Appearance.EyeHighlightStyle = QAFaceIndex;
		Appearance.BrowStyle = QAFaceIndex;
		Appearance.EyelashStyle = QAFaceIndex;
		Appearance.EyelineStyle = QAFaceIndex;
		Appearance.MouthStyle = QAFaceIndex;
		Appearance.LipStyle = QAFaceIndex;
		Appearance.MouthLineStyle = QAFaceIndex;
	}
	int32 QAHairIndex = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("VRoidQAHairIndex="), QAHairIndex))
	{
		Appearance.HairFrontStyle = QAHairIndex;
		Appearance.HairSideStyle = QAHairIndex;
		Appearance.HairBackStyle = QAHairIndex;
		Appearance.HairExtraStyle = QAHairIndex;
	}
	int32 QAHairBaseIndex = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("VRoidQAHairBaseIndex="), QAHairBaseIndex))
	{
		Appearance.HairBaseStyle = QAHairBaseIndex;
	}
	int32 QAHairExtraIndex = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("VRoidQAHairExtraIndex="), QAHairExtraIndex))
	{
		Appearance.HairExtraStyle = QAHairExtraIndex;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("VRoidQANoOutfit")))
	{
		Appearance.TopStyle = 0;
		Appearance.BottomStyle = 0;
		Appearance.OnepieceStyle = 0;
		Appearance.ShoesStyle = 0;
	}
	auto ParseStyleIndex = [](const TCHAR* Key, int32& Value)
	{
		int32 Parsed = INDEX_NONE;
		if (FParse::Value(FCommandLine::Get(), Key, Parsed))
		{
			Value = FMath::Max(0, Parsed);
			return true;
		}
		return false;
	};
	const bool bTopOverride = ParseStyleIndex(TEXT("VRoidQATopIndex="), Appearance.TopStyle);
	const bool bBottomOverride = ParseStyleIndex(TEXT("VRoidQABottomIndex="), Appearance.BottomStyle);
	const bool bDressOverride = ParseStyleIndex(TEXT("VRoidQADressIndex="), Appearance.OnepieceStyle);
	ParseStyleIndex(TEXT("VRoidQAShoesIndex="), Appearance.ShoesStyle);
	ParseStyleIndex(TEXT("VRoidQAHeadAccessoryIndex="), Appearance.HeadAccessoryStyle);
	ParseStyleIndex(TEXT("VRoidQAFaceAccessoryIndex="), Appearance.FaceAccessoryStyle);
	ParseStyleIndex(TEXT("VRoidQAEarAccessoryIndex="), Appearance.EarAccessoryStyle);
	ParseStyleIndex(TEXT("VRoidQATailAccessoryIndex="), Appearance.TailAccessoryStyle);
	ParseStyleIndex(TEXT("VRoidQANeckAccessoryIndex="), Appearance.NeckAccessoryStyle);
	if (bDressOverride && Appearance.OnepieceStyle > 0)
	{
		Appearance.TopStyle = 0;
		Appearance.BottomStyle = 0;
	}
	else if ((bTopOverride && Appearance.TopStyle > 0) ||
		(bBottomOverride && Appearance.BottomStyle > 0))
	{
		Appearance.OnepieceStyle = 0;
	}
	UpdateMeshes();
	CreateDynamicMaterials();
	ApplyAppearance(Appearance);
	if (FParse::Param(FCommandLine::Get(), TEXT("VRoidQANoHair")))
	{
		HairScalpMesh->SetVisibility(false, true);
		HairBaseMesh->SetVisibility(false, true);
		HairFrontMesh->SetVisibility(false, true);
		HairSideMesh->SetVisibility(false, true);
		HairBackMesh->SetVisibility(false, true);
		HairExtraMesh->SetVisibility(false, true);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("VRoidQAOnlyHairBase")))
	{
		HairFrontMesh->SetVisibility(false, true);
		HairSideMesh->SetVisibility(false, true);
		HairBackMesh->SetVisibility(false, true);
		HairExtraMesh->SetVisibility(false, true);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("VRoidQANoFaceDetails")))
	{
		EyeWhiteMesh->SetVisibility(false, true);
		EyeIrisMesh->SetVisibility(false, true);
		EyeHighlightMesh->SetVisibility(false, true);
		EyeExtraMesh->SetVisibility(false, true);
		BrowMesh->SetVisibility(false, true);
		EyelashMesh->SetVisibility(false, true);
		EyelineMesh->SetVisibility(false, true);
		MouthMesh->SetVisibility(false, true);
		LipOverlayMesh->SetVisibility(false, true);
		MouthLineOverlayMesh->SetVisibility(false, true);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("VRoidQAScreenshot")))
	{
		GetWorldTimerManager().SetTimer(QAScreenshotTimer, this, &ThisClass::CaptureQAScreenshot, 2.0f, false);
	}
}

void AUECustomizationPreviewActor::ApplyAppearance(const FUECharacterCustomizationData& NewAppearance)
{
	Appearance = NewAppearance;
	if (IsIdentityTint(Appearance.EyeColor))
	{
		Appearance.EyeColor = DefaultEyeIrisColor;
	}
	Appearance.Normalize();
	ClampAppearanceToCatalogs();
	UpdateMeshes();
	CreateDynamicMaterials();
	ApplyTransforms();
	ApplySelectedTextures();
	ApplyNeutralMaterialLighting();
	ApplyColors();
}

void AUECustomizationPreviewActor::RotatePreview(float DeltaYaw)
{
	CharacterRoot->AddLocalRotation(FRotator(0.0f, DeltaYaw, 0.0f));
}

void AUECustomizationPreviewActor::ResetAppearance()
{
	FUECharacterCustomizationData DefaultAppearance;
	DefaultAppearance.EyeColor = DefaultEyeIrisColor;
	ApplyAppearance(DefaultAppearance);
}

void AUECustomizationPreviewActor::RandomizeAppearance()
{
	FUECharacterCustomizationData RandomAppearance;
	RandomAppearance.Gender = FMath::RandBool() ? EUECharacterGender::Female : EUECharacterGender::Male;
	RandomAppearance.BodyPreset = static_cast<EUEBodyPreset>(FMath::RandRange(0, 2));
	RandomAppearance.Height = FMath::FRand();
	RandomAppearance.HeadSize = FMath::FRand();
	RandomAppearance.ShoulderWidth = FMath::FRand();

	RandomAppearance.SkinColor = FLinearColor::White;
	RandomAppearance.HairColor = FLinearColor::White;
	RandomAppearance.EyeColor = DefaultEyeIrisColor;
	RandomAppearance.LipColor = FLinearColor::White;
	RandomAppearance.OutfitColor = FLinearColor::White;
	RandomAppearance.TopColor = FLinearColor::White;
	RandomAppearance.BottomColor = FLinearColor::White;
	RandomAppearance.OnepieceColor = FLinearColor::White;
	RandomAppearance.ShoesColor = FLinearColor::White;
	RandomAppearance.AccessoryColor = FLinearColor::White;
	auto RandomIndex = [this](EUECustomizationPart Part)
	{
		const int32 DisplayIndex = FMath::RandRange(0, FMath::Max(GetOptionCount(Part) - 1, 0));
		return ResolveOptionIndex(Part, DisplayIndex);
	};
	RandomAppearance.FaceStyle = RandomIndex(EUECustomizationPart::FaceSkin);
	RandomAppearance.EyeWhiteStyle = RandomIndex(EUECustomizationPart::EyeWhite);
	RandomAppearance.EyeIrisStyle = RandomIndex(EUECustomizationPart::EyeIris);
	RandomAppearance.EyeHighlightStyle = RandomIndex(EUECustomizationPart::EyeHighlight);
	RandomAppearance.EyeExtraStyle = RandomIndex(EUECustomizationPart::EyeExtra);
	RandomAppearance.BrowStyle = RandomIndex(EUECustomizationPart::Brow);
	RandomAppearance.EyelashStyle = RandomIndex(EUECustomizationPart::Eyelash);
	RandomAppearance.EyelineStyle = RandomIndex(EUECustomizationPart::Eyeline);
	RandomAppearance.MouthStyle = RandomIndex(EUECustomizationPart::Mouth);
	RandomAppearance.LipStyle = RandomIndex(EUECustomizationPart::Lip);
	RandomAppearance.MouthLineStyle = RandomIndex(EUECustomizationPart::MouthLine);
	RandomAppearance.HairBaseStyle = 1;
	const int32 HairSetIndex = RandomIndex(EUECustomizationPart::HairSet);
	RandomAppearance.HairFrontStyle = HairSetIndex;
	RandomAppearance.HairSideStyle = HairSetIndex;
	RandomAppearance.HairBackStyle = HairSetIndex;
	RandomAppearance.HairExtraStyle = HairSetIndex;
	if (FMath::RandBool())
	{
		RandomAppearance.OnepieceStyle = FMath::Max(1, RandomIndex(EUECustomizationPart::Onepiece));
		RandomAppearance.TopStyle = 0;
		RandomAppearance.BottomStyle = 0;
	}
	else
	{
		RandomAppearance.OnepieceStyle = 0;
		RandomAppearance.TopStyle = FMath::Max(1, RandomIndex(EUECustomizationPart::Top));
		RandomAppearance.BottomStyle = FMath::Max(1, RandomIndex(EUECustomizationPart::Bottom));
	}
	RandomAppearance.ShoesStyle = FMath::Max(1, RandomIndex(EUECustomizationPart::Shoes));
	RandomAppearance.HeadAccessoryStyle = RandomIndex(EUECustomizationPart::HeadAccessory);
	RandomAppearance.FaceAccessoryStyle = RandomIndex(EUECustomizationPart::FaceAccessory);
	RandomAppearance.EarAccessoryStyle = RandomIndex(EUECustomizationPart::EarAccessory);
	RandomAppearance.TailAccessoryStyle = RandomIndex(EUECustomizationPart::TailAccessory);
	RandomAppearance.NeckAccessoryStyle = RandomIndex(EUECustomizationPart::NeckAccessory);
	ApplyAppearance(RandomAppearance);
}

bool AUECustomizationPreviewActor::SaveAppearance() const
{
	UUECharacterCustomizationSaveGame* SaveGame = Cast<UUECharacterCustomizationSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UUECharacterCustomizationSaveGame::StaticClass()));
	if (!SaveGame)
	{
		return false;
	}
	SaveGame->Appearance = Appearance;
	SaveGame->DataVersion = 15;
	return UGameplayStatics::SaveGameToSlot(SaveGame, SaveSlotName, SaveUserIndex);
}

bool AUECustomizationPreviewActor::LoadAppearance()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
	{
		return false;
	}
	const UUECharacterCustomizationSaveGame* SaveGame = Cast<UUECharacterCustomizationSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
	if (!SaveGame)
	{
		return false;
	}
	Appearance = SaveGame->Appearance;
	if (SaveGame->DataVersion < 3)
	{
		Appearance.FaceStyle = 0;
		Appearance.EyeWhiteStyle = 0;
		Appearance.EyeIrisStyle = 0;
		Appearance.EyeHighlightStyle = 0;
		Appearance.EyeExtraStyle = 0;
		Appearance.BrowStyle = 0;
		Appearance.EyelashStyle = 0;
		Appearance.EyelineStyle = 0;
		Appearance.MouthStyle = 0;
		Appearance.LipStyle = 0;
		Appearance.MouthLineStyle = 0;
		Appearance.EyeColor = DefaultEyeIrisColor;
	}
	if (SaveGame->DataVersion < 4 && Appearance.HairBaseStyle == 0)
	{
		Appearance.HairBaseStyle = 1;
	}
	if (SaveGame->DataVersion < 5 && Appearance.EyeColor.Equals(FLinearColor::White, 0.001f))
	{
		Appearance.EyeColor = FLinearColor(0.38f, 0.16f, 0.06f, 1.0f);
	}
	if (SaveGame->DataVersion < 6)
	{
		Appearance.LipColor = FLinearColor::FromSRGBColor(FColor(196, 102, 116));
	}
	if (SaveGame->DataVersion < 7)
	{
		++Appearance.TopStyle;
		++Appearance.BottomStyle;
		++Appearance.OnepieceStyle;
		++Appearance.ShoesStyle;
	}
	if (SaveGame->DataVersion < 8)
	{
		Appearance.HeadAccessoryStyle = 0;
		Appearance.FaceAccessoryStyle = 0;
		Appearance.EarAccessoryStyle = 0;
		Appearance.TailAccessoryStyle = 0;
		Appearance.NeckAccessoryStyle = 0;
	}
	if (SaveGame->DataVersion < 9)
	{
		Appearance.TopColor = Appearance.OutfitColor;
		Appearance.BottomColor = Appearance.OutfitColor;
		Appearance.OnepieceColor = Appearance.OutfitColor;
		Appearance.ShoesColor = Appearance.OutfitColor;
		Appearance.AccessoryColor = Appearance.OutfitColor;
	}
	if (SaveGame->DataVersion < 10)
	{
		if (Appearance.EyeIrisStyle <= 1)
		{
			Appearance.EyeIrisStyle = 2;
		}
		if (Appearance.EyeHighlightStyle <= 1)
		{
			Appearance.EyeHighlightStyle = 2;
		}
		if (Appearance.BrowStyle <= 1)
		{
			Appearance.BrowStyle = 2;
		}
		if (Appearance.EyelineStyle <= 1)
		{
			Appearance.EyelineStyle = 2;
		}
	}
	if (SaveGame->DataVersion < 11 &&
		Appearance.EyeColor.Equals(FLinearColor(0.38f, 0.16f, 0.06f, 1.0f), 0.01f))
	{
		Appearance.EyeColor = FLinearColor::FromSRGBColor(FColor(104, 154, 178));
	}
	if (SaveGame->DataVersion < 12)
	{
		Appearance.SkinColor = FLinearColor::White;
		Appearance.HairColor = FLinearColor::White;
		Appearance.LipColor = FLinearColor::White;
		Appearance.OutfitColor = FLinearColor::White;
		Appearance.TopColor = FLinearColor::White;
		Appearance.BottomColor = FLinearColor::White;
		Appearance.OnepieceColor = FLinearColor::White;
		Appearance.ShoesColor = FLinearColor::White;
		Appearance.AccessoryColor = FLinearColor::White;
	}
	if (SaveGame->DataVersion < 13 && IsIdentityTint(Appearance.EyeColor))
	{
		Appearance.EyeColor = DefaultEyeIrisColor;
	}
	if (SaveGame->DataVersion < 15)
	{
		Appearance.HairBaseStyle = 1;
	}
	Appearance.Normalize();
	return true;
}

void AUECustomizationPreviewActor::CreateMaterialsForComponent(
	USkeletalMeshComponent* Component,
	TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMaterials,
	UMaterialInterface* TemplateMaterial)
{
	if (!Component || !Component->GetSkeletalMeshAsset())
	{
		return;
	}
	Component->EmptyOverrideMaterials();
	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* SourceMaterial = Component->GetMaterial(Index);
		if (UMaterialInterface* Parent = TemplateMaterial ? TemplateMaterial : SourceMaterial)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this);
			if (TemplateMaterial)
			{
				PreserveSourceMaterialTextures(SourceMaterial, Material);
			}
			Component->SetMaterial(Index, Material);
			OutMaterials.Add(Material);
		}
	}
}

void AUECustomizationPreviewActor::CreateMaskedOverlayMaterials(
	USkeletalMeshComponent* Component,
	TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMaterials,
	UMaterialInterface* TemplateMaterial)
{
	if (!Component || !Component->GetSkeletalMeshAsset())
	{
		return;
	}
	Component->EmptyOverrideMaterials();
	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* SourceMaterial = TemplateMaterial
			? TemplateMaterial
			: Component->GetMaterial(Index);
		if (!SourceMaterial)
		{
			continue;
		}
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(SourceMaterial, this);
		PreserveSourceMaterialTextures(Component->GetMaterial(Index), Material);
		Component->SetMaterial(Index, Material);
		OutMaterials.Add(Material);
	}
}

void AUECustomizationPreviewActor::CreateOpaqueFaceMaterials(
	USkeletalMeshComponent* Component,
	TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMaterials,
	UMaterialInterface* TemplateMaterial)
{
	if (!Component || !Component->GetSkeletalMeshAsset())
	{
		return;
	}
	Component->EmptyOverrideMaterials();
	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* SourceMaterial = TemplateMaterial
			? TemplateMaterial
			: Component->GetMaterial(Index);
		if (!SourceMaterial)
		{
			continue;
		}
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(SourceMaterial, this);
		PreserveSourceMaterialTextures(Component->GetMaterial(Index), Material);
		Component->SetMaterial(Index, Material);
		OutMaterials.Add(Material);
	}
}

void AUECustomizationPreviewActor::CreateDynamicMaterials()
{
	BodySkinMaterials.Empty();
	FaceSkinMaterials.Empty();
	HairScalpMaterials.Empty();
	HairBaseMaterials.Empty();
	HairMaterials.Empty();
	EyeMaterials.Empty();
	EyeWhiteMaterials.Empty();
	EyeHighlightMaterials.Empty();
	EyeExtraMaterials.Empty();
	BrowMaterials.Empty();
	EyelashMaterials.Empty();
	EyelineMaterials.Empty();
	MouthMaterials.Empty();
	LipMaterials.Empty();
	MouthLineMaterials.Empty();
	TopMaterials.Empty();
	BottomMaterials.Empty();
	OnepieceMaterials.Empty();
	ShoesMaterials.Empty();
	OutfitMaterials.Empty();
	HeadAccessoryMaterials.Empty();
	FaceAccessoryMaterials.Empty();
	EarAccessoryMaterials.Empty();
	TailAccessoryMaterials.Empty();
	NeckAccessoryMaterials.Empty();
	AccessoryMaterials.Empty();

	UMaterialInterface* OutfitTemplateMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/CharacterCustomization/Materials/M_UEOutfitTextured.M_UEOutfitTextured"));
	UMaterialInterface* EyeWhiteTemplateMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/CharacterCustomization/Materials/M_UEEyeWhiteSolid.M_UEEyeWhiteSolid"));
	UMaterialInterface* IrisTemplateMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/CharacterCustomization/Materials/M_UEIrisTintMasked.M_UEIrisTintMasked"));
	CreateMaterialsForComponent(BodyMesh, BodySkinMaterials, OutfitTemplateMaterial);
	CreateMaterialsForComponent(FaceSkinMesh, FaceSkinMaterials);
	CreateMaterialsForComponent(EyeIrisMesh, EyeMaterials, IrisTemplateMaterial);
	CreateMaterialsForComponent(EyeWhiteMesh, EyeWhiteMaterials, EyeWhiteTemplateMaterial);
	CreateMaterialsForComponent(EyeHighlightMesh, EyeHighlightMaterials);
	CreateMaterialsForComponent(EyeExtraMesh, EyeExtraMaterials);
	CreateMaterialsForComponent(BrowMesh, BrowMaterials);
	CreateMaterialsForComponent(EyelashMesh, EyelashMaterials);
	CreateMaterialsForComponent(EyelineMesh, EyelineMaterials);
	CreateMaterialsForComponent(MouthMesh, MouthMaterials);
	CreateMaskedOverlayMaterials(
		LipOverlayMesh,
		LipMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/Mouth.Mouth")));
	CreateMaskedOverlayMaterials(
		MouthLineOverlayMesh,
		MouthLineMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/Mouth.Mouth")));
	CreateMaterialsForComponent(HairScalpMesh, HairScalpMaterials, OutfitTemplateMaterial);
	CreateMaterialsForComponent(HairBaseMesh, HairBaseMaterials);
	HairMaterials.Append(HairBaseMaterials);
	CreateMaterialsForComponent(HairFrontMesh, HairMaterials);
	CreateMaterialsForComponent(HairSideMesh, HairMaterials);
	CreateMaterialsForComponent(HairBackMesh, HairMaterials);
	CreateMaterialsForComponent(HairExtraMesh, HairMaterials);
	CreateMaterialsForComponent(TopMesh, TopMaterials);
	CreateMaterialsForComponent(BottomMesh, BottomMaterials);
	CreateMaterialsForComponent(OnepieceMesh, OnepieceMaterials);
	CreateMaterialsForComponent(ShoesMesh, ShoesMaterials);
	OutfitMaterials.Append(TopMaterials);
	OutfitMaterials.Append(BottomMaterials);
	OutfitMaterials.Append(OnepieceMaterials);
	OutfitMaterials.Append(ShoesMaterials);
	CreateMaterialsForComponent(HeadAccessoryMesh, HeadAccessoryMaterials);
	CreateMaterialsForComponent(FaceAccessoryMesh, FaceAccessoryMaterials);
	CreateMaterialsForComponent(EarAccessoryMesh, EarAccessoryMaterials);
	CreateMaterialsForComponent(TailAccessoryMesh, TailAccessoryMaterials);
	CreateMaterialsForComponent(NeckAccessoryMesh, NeckAccessoryMaterials);
	AccessoryMaterials.Append(HeadAccessoryMaterials);
	AccessoryMaterials.Append(FaceAccessoryMaterials);
	AccessoryMaterials.Append(EarAccessoryMaterials);
	AccessoryMaterials.Append(TailAccessoryMaterials);
	AccessoryMaterials.Append(NeckAccessoryMaterials);

	for (UMaterialInstanceDynamic* Material : OutfitMaterials)
	{
		if (!Material)
		{
			continue;
		}
		Material->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
		Material->SetScalarParameterValue(TEXT("OpacityMask"), 1.0f);
		const float OpacityWeight = Material->K2_GetScalarParameterValue(TEXT("OpacityMaskMapWeight"));
		Material->SetScalarParameterValue(TEXT("OpacityMaskMapWeight"), OpacityWeight > 0.5f ? 1.0f : 0.0f);
	}
	ApplySelectedTextures();
	ApplyNeutralMaterialLighting();
}

void AUECustomizationPreviewActor::ApplySelectedTextures()
{
	auto Apply = [this](
		EUECustomizationPart Part,
		int32 Index,
		USkeletalMeshComponent* Component,
		const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials,
		bool bKeepCoreGeometryVisible)
	{
		UTexture2D* Texture = GetOptionTexture(Part, Index);
		UTexture2D* NormalTexture = Part == EUECustomizationPart::FaceSkin &&
			FaceSkinNormalTextureCatalog.IsValidIndex(Index)
			? FaceSkinNormalTextureCatalog[Index].Get()
			: nullptr;
		if (Component)
		{
			Component->SetVisibility(bKeepCoreGeometryVisible || Texture != nullptr, true);
		}
		if (!Texture)
		{
			for (UMaterialInstanceDynamic* Material : Materials)
			{
				if (!Material)
				{
					continue;
				}
				Material->SetScalarParameterValue(TEXT("DiffuseColorMapWeight"), 0.0f);
				Material->SetScalarParameterValue(TEXT("OpacityMaskMapWeight"), bKeepCoreGeometryVisible ? 0.0f : 1.0f);
				Material->SetScalarParameterValue(TEXT("EmissiveColorMapWeight"), 0.0f);
				Material->SetScalarParameterValue(TEXT("NormalMapWeight"), 0.0f);
			}
			return;
		}
		for (UMaterialInstanceDynamic* Material : Materials)
		{
			if (!Material)
			{
				continue;
			}
			Material->SetTextureParameterValue(TEXT("DiffuseColorMap"), Texture);
			Material->SetTextureParameterValue(TEXT("OpacityMaskMap"), Texture);
			Material->SetTextureParameterValue(TEXT("EmissiveColorMap"), Texture);
			Material->SetScalarParameterValue(TEXT("DiffuseColorMapWeight"), 1.0f);
			Material->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
			Material->SetScalarParameterValue(TEXT("OpacityMask"), 1.0f);
			Material->SetScalarParameterValue(TEXT("OpacityMaskMapWeight"), bKeepCoreGeometryVisible ? 0.0f : 1.0f);
			Material->SetScalarParameterValue(TEXT("NormalMapWeight"), NormalTexture ? 1.0f : 0.0f);
			if (NormalTexture)
			{
				Material->SetTextureParameterValue(TEXT("NormalMap"), NormalTexture);
			}
		}
	};

	Apply(EUECustomizationPart::FaceSkin, Appearance.FaceStyle, FaceSkinMesh, FaceSkinMaterials, true);
	Apply(EUECustomizationPart::EyeWhite, Appearance.EyeWhiteStyle, EyeWhiteMesh, EyeWhiteMaterials, false);
	Apply(EUECustomizationPart::EyeIris, Appearance.EyeIrisStyle, EyeIrisMesh, EyeMaterials, false);
	Apply(EUECustomizationPart::EyeHighlight, Appearance.EyeHighlightStyle, EyeHighlightMesh, EyeHighlightMaterials, false);
	Apply(EUECustomizationPart::Brow, Appearance.BrowStyle, BrowMesh, BrowMaterials, false);
	Apply(EUECustomizationPart::Eyelash, Appearance.EyelashStyle, EyelashMesh, EyelashMaterials, false);
	Apply(EUECustomizationPart::Eyeline, Appearance.EyelineStyle, EyelineMesh, EyelineMaterials, false);
	Apply(EUECustomizationPart::Mouth, Appearance.MouthStyle, MouthMesh, MouthMaterials, false);
	Apply(EUECustomizationPart::Lip, Appearance.LipStyle, LipOverlayMesh, LipMaterials, false);
	Apply(EUECustomizationPart::MouthLine, Appearance.MouthLineStyle, MouthLineOverlayMesh, MouthLineMaterials, false);
}

void AUECustomizationPreviewActor::ApplyColors()
{
	auto PreserveMaskedLayerTexture = [](
		const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials)
	{
		SetMaterialScalar(Materials, TEXT("DiffuseColorMapWeight"), 1.0f);
		SetMaterialScalar(Materials, TEXT("Opacity"), 1.0f);
		SetMaterialScalar(Materials, TEXT("OpacityMask"), 1.0f);
		SetMaterialScalar(Materials, TEXT("OpacityMaskMapWeight"), 1.0f);
		SetMaterialScalar(Materials, TEXT("EmissiveColorMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("SpecularColorMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("Shininess"), 0.0f);
	};
	auto ApplyOutfitMaterial = [](const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, const FLinearColor& Color)
	{
		if (!IsIdentityTint(Color))
		{
			const FLinearColor TintColor = Color.GetClamped();
			SetMaterialColor(Materials, DiffuseColorParameter, TintColor);
			SetMaterialColor(Materials, TEXT("BaseColor"), TintColor);
			SetMaterialColor(Materials, TEXT("Base Color"), TintColor);
			SetMaterialColor(Materials, TEXT("AmbientColor"), TintColor);
			SetMaterialColor(Materials, TEXT("TintColor"), TintColor);
			SetMaterialColor(Materials, TEXT("Color"), TintColor);
			SetMaterialColor(Materials, TEXT("CustomTint"), TintColor);
		}
		PreserveRuntimeTextureWeights(Materials);
		SetMaterialScalar(Materials, TEXT("TextureEmissionWeight"), 0.08f);
	};
	auto ApplyAccessoryTint = [](const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, const FLinearColor& Color)
	{
		if (!IsIdentityTint(Color))
		{
			SetMaterialColor(Materials, DiffuseColorParameter, Color);
			SetMaterialColor(Materials, TEXT("BaseColor"), Color);
			SetMaterialColor(Materials, TEXT("Base Color"), Color);
			SetMaterialColor(Materials, TEXT("AmbientColor"), Color);
			SetMaterialColor(Materials, TEXT("TintColor"), Color);
			SetMaterialColor(Materials, TEXT("Color"), Color);
			SetMaterialColor(Materials, TEXT("AccessoryColor"), Color);
			SetMaterialColor(Materials, TEXT("CustomTint"), Color);
			SetMaterialColor(Materials, TEXT("EmissiveColor"), Color * 0.08f);
		}
		SetMaterialColor(Materials, TEXT("SpecularColor"), FLinearColor::Black);
		PreserveRuntimeTextureWeights(Materials);
		SetMaterialScalar(Materials, TEXT("SpecularColorMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("Shininess"), 0.0f);
		SetMaterialScalar(Materials, TEXT("TextureEmissionWeight"), 0.08f);
	};
	const FLinearColor SkinColor = Appearance.SkinColor;
	const FLinearColor EffectiveBodySkinColor = IsIdentityTint(SkinColor) ? DefaultBodySkinColor : SkinColor.GetClamped();
	const FLinearColor BrowColor(
		0.26f * Appearance.HairColor.R,
		0.075f * Appearance.HairColor.G,
		0.035f * Appearance.HairColor.B,
		1.0f);
	const FLinearColor LashColor(
		0.055f * Appearance.HairColor.R,
		0.018f * Appearance.HairColor.G,
		0.012f * Appearance.HairColor.B,
		1.0f);
	SetMaterialColor(BodySkinMaterials, DiffuseColorParameter, EffectiveBodySkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("BaseColor"), EffectiveBodySkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("Base Color"), EffectiveBodySkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("AmbientColor"), EffectiveBodySkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("EmissiveColor"), EffectiveBodySkinColor * 0.35f);
	SetMaterialColor(BodySkinMaterials, TEXT("TintColor"), EffectiveBodySkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("Color"), EffectiveBodySkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("OriginalTint"), EffectiveBodySkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("CustomTint"), FLinearColor::White);
	if (UTexture* WhiteTexture = LoadObject<UTexture>(nullptr, WhiteTexturePath))
	{
		for (UMaterialInstanceDynamic* Material : BodySkinMaterials)
		{
			if (!Material)
			{
				continue;
			}
			Material->SetTextureParameterValue(TEXT("DiffuseColorMap"), WhiteTexture);
			Material->SetTextureParameterValue(TEXT("EmissiveColorMap"), WhiteTexture);
			Material->SetTextureParameterValue(TEXT("OpacityMaskMap"), WhiteTexture);
		}
	}
	if (UTexture* BodyNormalTexture = LoadObject<UTexture>(nullptr, FemaleBodyNormalTexturePath))
	{
		for (UMaterialInstanceDynamic* Material : BodySkinMaterials)
		{
			if (!Material)
			{
				continue;
			}
			Material->SetTextureParameterValue(TEXT("NormalMap"), BodyNormalTexture);
			Material->SetScalarParameterValue(TEXT("NormalMapWeight"), 0.0f);
			Material->SetScalarParameterValue(TEXT("NormalStrength"), 0.0f);
		}
	}
	SetMaterialScalar(BodySkinMaterials, TEXT("DiffuseColorMapWeight"), 0.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("Opacity"), 1.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("OpacityMaskMapWeight"), 1.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("EmissiveColorMapWeight"), 0.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("TextureEmissionWeight"), 0.85f);
	SetMaterialScalar(BodySkinMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("Shininess"), 0.0f);
	if (!IsIdentityTint(SkinColor))
	{
		SetMaterialColor(FaceSkinMaterials, DiffuseColorParameter, SkinColor);
		SetMaterialColor(FaceSkinMaterials, TEXT("BaseColor"), SkinColor);
		SetMaterialColor(FaceSkinMaterials, TEXT("Base Color"), SkinColor);
		SetMaterialColor(FaceSkinMaterials, TEXT("AmbientColor"), SkinColor);
		SetMaterialColor(FaceSkinMaterials, TEXT("EmissiveColor"), SkinColor);
		SetMaterialColor(FaceSkinMaterials, TEXT("TintColor"), SkinColor);
		SetMaterialColor(FaceSkinMaterials, TEXT("Color"), SkinColor);
	}
	SetMaterialScalar(FaceSkinMaterials, TEXT("DiffuseColorMapWeight"), 1.0f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("Opacity"), 1.0f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("OpacityMaskMapWeight"), 0.0f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("EmissiveColorMapWeight"), 0.2f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("Shininess"), 0.0f);
	if (!IsIdentityTint(Appearance.HairColor))
	{
		SetMaterialColor(HairMaterials, DiffuseColorParameter, Appearance.HairColor);
		SetMaterialColor(HairMaterials, TEXT("BaseColor"), Appearance.HairColor);
		SetMaterialColor(HairMaterials, TEXT("Base Color"), Appearance.HairColor);
		SetMaterialColor(HairMaterials, TEXT("AmbientColor"), Appearance.HairColor);
		SetMaterialColor(HairMaterials, TEXT("TintColor"), Appearance.HairColor);
		SetMaterialColor(HairMaterials, TEXT("Color"), Appearance.HairColor);
	}
	SetMaterialScalar(HairMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(HairMaterials, TEXT("Shininess"), 0.0f);
	PreserveRuntimeTextureWeights(HairMaterials);
	SetMaterialScalar(HairMaterials, TEXT("TextureEmissionWeight"), 0.18f);

	const FLinearColor HairUnderlayColor = IsIdentityTint(Appearance.HairColor)
		? FLinearColor(0.11f, 0.035f, 0.018f, 1.0f)
		: Appearance.HairColor.GetClamped();
	SetMaterialColor(HairScalpMaterials, DiffuseColorParameter, HairUnderlayColor);
	SetMaterialColor(HairScalpMaterials, TEXT("BaseColor"), HairUnderlayColor);
	SetMaterialColor(HairScalpMaterials, TEXT("Base Color"), HairUnderlayColor);
	SetMaterialColor(HairScalpMaterials, TEXT("AmbientColor"), HairUnderlayColor);
	SetMaterialColor(HairScalpMaterials, TEXT("TintColor"), HairUnderlayColor);
	SetMaterialColor(HairScalpMaterials, TEXT("Color"), HairUnderlayColor);
	SetMaterialScalar(HairScalpMaterials, TEXT("DiffuseColorMapWeight"), 0.0f);
	SetMaterialScalar(HairScalpMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(HairScalpMaterials, TEXT("OpacityMaskMapWeight"), 0.0f);
	SetMaterialScalar(HairScalpMaterials, TEXT("TextureEmissionWeight"), 0.08f);
	const FLinearColor EyeSource = Appearance.EyeColor.GetClamped();
	const bool bUseEyeTint = !IsIdentityTint(EyeSource);
	const FLinearColor IrisLightColor = bUseEyeTint
		? FLinearColor(
			FMath::Clamp(EyeSource.R * 2.0f + 0.12f, 0.0f, 1.0f),
			FMath::Clamp(EyeSource.G * 2.0f + 0.12f, 0.0f, 1.0f),
			FMath::Clamp(EyeSource.B * 2.0f + 0.12f, 0.0f, 1.0f),
			1.0f)
		: FLinearColor::White;
	if (bUseEyeTint)
	{
		SetMaterialColor(EyeMaterials, DiffuseColorParameter, IrisLightColor);
		SetMaterialColor(EyeMaterials, TEXT("BaseColor"), IrisLightColor);
		SetMaterialColor(EyeMaterials, TEXT("Base Color"), IrisLightColor);
		SetMaterialColor(EyeMaterials, TEXT("AmbientColor"), IrisLightColor);
		SetMaterialColor(EyeMaterials, TEXT("EmissiveColor"), IrisLightColor);
		SetMaterialColor(EyeMaterials, TEXT("TintColor"), IrisLightColor);
		SetMaterialColor(EyeMaterials, TEXT("Color"), IrisLightColor);
		SetMaterialColor(EyeMaterials, TEXT("IrisColor"), IrisLightColor);
		SetMaterialColor(EyeMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	}
	SetMaterialScalar(EyeMaterials, TEXT("DiffuseColorMapWeight"), 1.0f);
	SetMaterialScalar(EyeMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(EyeMaterials, TEXT("OpacityMaskMapWeight"), 1.0f);
	SetMaterialScalar(EyeMaterials, TEXT("EmissiveColorMapWeight"), 0.48f);
	SetMaterialScalar(EyeMaterials, TEXT("IrisFill"), bUseEyeTint ? 0.62f : 0.0f);
	SetMaterialColor(EyeWhiteMaterials, TEXT("EyeWhiteTint"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, DiffuseColorParameter, FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("BaseColor"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("Base Color"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("AmbientColor"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("EmissiveColor"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("TintColor"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("Color"), FLinearColor::White);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("DiffuseColorMapWeight"), 1.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("Opacity"), 1.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("OpacityMaskMapWeight"), 1.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("EmissiveColorMapWeight"), 0.22f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("Shininess"), 0.0f);
	SetMaterialColor(EyeHighlightMaterials, DiffuseColorParameter, FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("BaseColor"), FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("Base Color"), FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("AmbientColor"), FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("EmissiveColor"), FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("TintColor"), FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("Color"), FLinearColor::White);
	if (bUseEyeTint)
	{
		SetMaterialColor(EyeExtraMaterials, DiffuseColorParameter, EyeSource);
		SetMaterialColor(EyeExtraMaterials, TEXT("BaseColor"), EyeSource);
		SetMaterialColor(EyeExtraMaterials, TEXT("Base Color"), EyeSource);
		SetMaterialColor(EyeExtraMaterials, TEXT("AmbientColor"), EyeSource);
		SetMaterialColor(EyeExtraMaterials, TEXT("EmissiveColor"), EyeSource);
		SetMaterialColor(EyeExtraMaterials, TEXT("TintColor"), EyeSource);
		SetMaterialColor(EyeExtraMaterials, TEXT("Color"), EyeSource);
	}
	if (!IsIdentityTint(Appearance.HairColor))
	{
		SetMaterialColor(BrowMaterials, DiffuseColorParameter, BrowColor);
		SetMaterialColor(BrowMaterials, TEXT("BaseColor"), BrowColor);
		SetMaterialColor(BrowMaterials, TEXT("AmbientColor"), BrowColor);
		SetMaterialColor(BrowMaterials, TEXT("EmissiveColor"), BrowColor);
		SetMaterialColor(BrowMaterials, TEXT("TintColor"), BrowColor);
		SetMaterialColor(EyelashMaterials, DiffuseColorParameter, LashColor);
		SetMaterialColor(EyelashMaterials, TEXT("BaseColor"), LashColor);
		SetMaterialColor(EyelashMaterials, TEXT("AmbientColor"), LashColor);
		SetMaterialColor(EyelashMaterials, TEXT("EmissiveColor"), LashColor);
		SetMaterialColor(EyelashMaterials, TEXT("TintColor"), LashColor);
		SetMaterialColor(EyelineMaterials, DiffuseColorParameter, LashColor);
		SetMaterialColor(EyelineMaterials, TEXT("BaseColor"), LashColor);
		SetMaterialColor(EyelineMaterials, TEXT("AmbientColor"), LashColor);
		SetMaterialColor(EyelineMaterials, TEXT("EmissiveColor"), LashColor);
		SetMaterialColor(EyelineMaterials, TEXT("TintColor"), LashColor);
	}
	PreserveMaskedLayerTexture(BrowMaterials);
	PreserveMaskedLayerTexture(EyelashMaterials);
	PreserveMaskedLayerTexture(EyelineMaterials);
	SetMaterialScalar(MouthMaterials, TEXT("Opacity"), 1.0f);
	SetMaterialScalar(MouthMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(MouthMaterials, TEXT("OpacityMaskMapWeight"), 1.0f);
	SetMaterialScalar(MouthMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(MouthMaterials, TEXT("Shininess"), 0.0f);
	const bool bUseLipTint = !IsIdentityTint(Appearance.LipColor);
	if (bUseLipTint)
	{
		SetMaterialColor(LipMaterials, DiffuseColorParameter, Appearance.LipColor);
		SetMaterialColor(LipMaterials, TEXT("BaseColor"), Appearance.LipColor);
		SetMaterialColor(LipMaterials, TEXT("AmbientColor"), Appearance.LipColor);
		SetMaterialColor(LipMaterials, TEXT("EmissiveColor"), Appearance.LipColor);
		SetMaterialColor(LipMaterials, TEXT("TintColor"), Appearance.LipColor);
	}
	SetMaterialScalar(LipMaterials, TEXT("DiffuseColorMapWeight"), 1.0f);
	SetMaterialScalar(LipMaterials, TEXT("EmissiveColorMapWeight"), 0.0f);
	SetMaterialScalar(LipMaterials, TEXT("OpacityMaskMapWeight"), 1.0f);
	SetMaterialScalar(LipMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(LipMaterials, TEXT("Shininess"), 0.0f);
	const FLinearColor MouthLineColor(
		Appearance.LipColor.R * 0.55f,
		Appearance.LipColor.G * 0.55f,
		Appearance.LipColor.B * 0.55f,
		Appearance.LipColor.A);
	if (bUseLipTint)
	{
		SetMaterialColor(MouthLineMaterials, DiffuseColorParameter, MouthLineColor);
		SetMaterialColor(MouthLineMaterials, TEXT("BaseColor"), MouthLineColor);
		SetMaterialColor(MouthLineMaterials, TEXT("AmbientColor"), MouthLineColor);
		SetMaterialColor(MouthLineMaterials, TEXT("EmissiveColor"), MouthLineColor);
		SetMaterialColor(MouthLineMaterials, TEXT("TintColor"), MouthLineColor);
	}
	SetMaterialScalar(MouthLineMaterials, TEXT("DiffuseColorMapWeight"), 1.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("EmissiveColorMapWeight"), 0.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("OpacityMaskMapWeight"), 1.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("Shininess"), 0.0f);
	ApplyOutfitMaterial(TopMaterials, Appearance.TopColor);
	ApplyOutfitMaterial(BottomMaterials, Appearance.BottomColor);
	ApplyOutfitMaterial(OnepieceMaterials, Appearance.OnepieceColor);
	ApplyOutfitMaterial(ShoesMaterials, Appearance.ShoesColor);
	ApplyAccessoryTint(HeadAccessoryMaterials, Appearance.AccessoryColor);
	ApplyAccessoryTint(FaceAccessoryMaterials, Appearance.AccessoryColor);
	ApplyAccessoryTint(EarAccessoryMaterials, Appearance.HairColor);
	ApplyAccessoryTint(TailAccessoryMaterials, Appearance.HairColor);
	ApplyAccessoryTint(NeckAccessoryMaterials, Appearance.AccessoryColor);
	SetMaterialScalar(FaceAccessoryMaterials, TEXT("Opacity"), 1.0f);
}

void AUECustomizationPreviewActor::ApplyNeutralMaterialLighting()
{
	auto ApplyTextureEmission = [](const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, float Strength)
	{
		for (UMaterialInstanceDynamic* Material : Materials)
		{
			if (!Material)
			{
				continue;
			}
			if (UTexture* DiffuseTexture = Material->K2_GetTextureParameterValue(TEXT("DiffuseColorMap")))
			{
				Material->SetTextureParameterValue(TEXT("EmissiveColorMap"), DiffuseTexture);
				Material->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::White);
				Material->SetScalarParameterValue(TEXT("EmissiveColorMapWeight"), Strength);
			}
		}
	};

	ApplyTextureEmission(BodySkinMaterials, 0.82f);
	ApplyTextureEmission(FaceSkinMaterials, 1.05f);
	ApplyTextureEmission(EyeWhiteMaterials, 0.55f);
	ApplyTextureEmission(EyeHighlightMaterials, 1.2f);
	ApplyTextureEmission(EyeExtraMaterials, 0.65f);
	ApplyTextureEmission(BrowMaterials, 0.28f);
	ApplyTextureEmission(EyelashMaterials, 0.18f);
	ApplyTextureEmission(EyelineMaterials, 0.18f);
	ApplyTextureEmission(MouthMaterials, 0.5f);
	ApplyTextureEmission(LipMaterials, 0.55f);
	ApplyTextureEmission(MouthLineMaterials, 0.35f);
	ApplyTextureEmission(EyeMaterials, 0.75f);
	ApplyTextureEmission(HairMaterials, 1.0f);
	ApplyTextureEmission(OutfitMaterials, 0.12f);
	ApplyTextureEmission(AccessoryMaterials, 0.12f);
}

void AUECustomizationPreviewActor::ApplyTransforms()
{
	const float PresetWidth[] = {0.94f, 1.0f, 1.06f};
	const int32 PresetIndex = FMath::Clamp(static_cast<int32>(Appearance.BodyPreset), 0, 2);
	const float WidthScale = PresetWidth[PresetIndex] * FMath::Lerp(0.96f, 1.04f, Appearance.ShoulderWidth);
	const float HeightScale = FMath::Lerp(0.94f, 1.06f, Appearance.Height);
	CharacterRoot->SetRelativeScale3D(FVector(WidthScale, FMath::Sqrt(WidthScale), HeightScale));

	const TArray<USkeletalMeshComponent*> ModularParts = {
		BodyMesh, FaceSkinMesh, EyeWhiteMesh, EyeIrisMesh, EyeHighlightMesh, EyeExtraMesh, BrowMesh, EyelashMesh,
		EyelineMesh, MouthMesh, LipOverlayMesh, MouthLineOverlayMesh,
		HairScalpMesh, HairBaseMesh, HairFrontMesh, HairSideMesh, HairBackMesh, HairExtraMesh,
		TopMesh, BottomMesh, OnepieceMesh, ShoesMesh,
		HeadAccessoryMesh, FaceAccessoryMesh, EarAccessoryMesh, TailAccessoryMesh, NeckAccessoryMesh};
	for (USkeletalMeshComponent* Part : ModularParts)
	{
		if (!Part)
		{
			continue;
		}
		if (Part != BodyMesh && Part->GetAttachParent() != CharacterRoot)
		{
			Part->AttachToComponent(CharacterRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
		Part->SetRelativeLocation(FVector::ZeroVector);
		Part->SetRelativeRotation(FRotator::ZeroRotator);
		Part->SetRelativeScale3D(FVector::OneVector);
	}

	const bool bFemale = Appearance.Gender == EUECharacterGender::Female;
	const float HeadPivotY = CatalogAsset
		? (bFemale ? CatalogAsset->FemaleHeadPivotY : CatalogAsset->MaleHeadPivotY)
		: (bFemale ? 4.15f : 5.5f);
	const float HeadPivotZ = CatalogAsset
		? ((bFemale ? CatalogAsset->FemaleHeadPivotZ : CatalogAsset->MaleHeadPivotZ) +
			(bFemale ? CatalogAsset->FemaleHeadVerticalOffset : CatalogAsset->MaleHeadVerticalOffset))
		: (bFemale ? 143.0f : 162.0f);
	const FVector HeadPivot(0.0f, HeadPivotY, HeadPivotZ);
	const float HeadScale = FMath::Lerp(0.92f, 1.08f, Appearance.HeadSize);
	const float HairScale = HeadScale * (CatalogAsset ? CatalogAsset->HairRadialScale : 1.02f);
	const float HairUnderlayScale = HairScale * (CatalogAsset ? CatalogAsset->HairScalpInsetScale : 0.985f);

	const TArray<USkeletalMeshComponent*> HeadParts = {
		FaceSkinMesh, EyeWhiteMesh, EyeIrisMesh, EyeHighlightMesh, EyeExtraMesh, BrowMesh, EyelashMesh,
		EyelineMesh, MouthMesh, LipOverlayMesh, MouthLineOverlayMesh,
		HeadAccessoryMesh, FaceAccessoryMesh, EarAccessoryMesh};
	for (USkeletalMeshComponent* Part : HeadParts)
	{
		ScaleComponentAroundPivot(Part, HeadPivot, HeadScale);
	}
	ScaleComponentAroundPivot(HairScalpMesh, HeadPivot, HairUnderlayScale);
	for (USkeletalMeshComponent* Part : {HairBaseMesh, HairFrontMesh, HairSideMesh, HairBackMesh, HairExtraMesh})
	{
		ScaleComponentAroundPivot(Part, HeadPivot, HairScale);
	}

	const float FaceLayerDepth = FMath::Clamp(CatalogAsset ? CatalogAsset->ScleraDepthOffsetY : 0.005f, 0.002f, 0.012f);
	auto OffsetFaceLayer = [FaceLayerDepth, HeadScale](USkeletalMeshComponent* Component, float Order)
	{
		if (Component)
		{
			Component->AddLocalOffset(FVector(0.0f, FaceLayerDepth * Order * HeadScale, 0.0f));
		}
	};
	OffsetFaceLayer(EyeWhiteMesh, 1.0f);
	OffsetFaceLayer(EyeIrisMesh, 2.0f);
	OffsetFaceLayer(EyeHighlightMesh, 3.0f);
	OffsetFaceLayer(EyeExtraMesh, 3.5f);
	OffsetFaceLayer(BrowMesh, 4.0f);
	OffsetFaceLayer(EyelashMesh, 5.0f);
	OffsetFaceLayer(EyelineMesh, 6.0f);
	OffsetFaceLayer(MouthMesh, 4.0f);
	OffsetFaceLayer(LipOverlayMesh, 5.0f);
	OffsetFaceLayer(MouthLineOverlayMesh, 6.0f);

	if (HeadAccessoryMesh)
	{
		const float HeadAccessoryZ = CatalogAsset ? CatalogAsset->HeadAccessoryVerticalOffset : 9.0f;
		HeadAccessoryMesh->AddLocalOffset(FVector(0.0f, 0.0f, HeadAccessoryZ * HeadScale));
	}
	if (FaceAccessoryMesh)
	{
		const float FaceAccessoryY = CatalogAsset ? CatalogAsset->FaceAccessoryForwardOffset : 6.5f;
		const float FaceAccessoryZ = CatalogAsset ? CatalogAsset->FaceAccessoryVerticalOffset : 17.5f;
		FaceAccessoryMesh->AddLocalOffset(FVector(0.0f, FaceAccessoryY * HeadScale, FaceAccessoryZ * HeadScale));
	}

}

void AUECustomizationPreviewActor::HideLegacyPreviewParts()
{
	TArray<UStaticMeshComponent*> LegacyParts;
	GetComponents<UStaticMeshComponent>(LegacyParts);
	for (UStaticMeshComponent* LegacyPart : LegacyParts)
	{
		LegacyPart->SetVisibility(false, true);
		LegacyPart->SetHiddenInGame(true);
	}
}

void AUECustomizationPreviewActor::CaptureQAScreenshot()
{
	UWidgetLayoutLibrary::RemoveAllWidgets(GetWorld());
	TArray<UUserWidget*> QAWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		GetWorld(), QAWidgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* Widget : QAWidgets)
	{
		Widget->SetVisibility(ESlateVisibility::Collapsed);
		Widget->RemoveFromParent();
	}
	UE_LOG(LogTemp, Display, TEXT("VRoid QA hid %d widgets"), QAWidgets.Num());
	UE_LOG(LogTemp, Display, TEXT("VRoid QA colors skin=%s hair=%s eye=%s lip=%s top=%s bottom=%s onepiece=%s shoes=%s accessory=%s"),
		*Appearance.SkinColor.ToString(),
		*Appearance.HairColor.ToString(),
		*Appearance.EyeColor.ToString(),
		*Appearance.LipColor.ToString(),
		*Appearance.TopColor.ToString(),
		*Appearance.BottomColor.ToString(),
		*Appearance.OnepieceColor.ToString(),
		*Appearance.ShoesColor.ToString(),
		*Appearance.AccessoryColor.ToString());

	auto LogPart = [](const TCHAR* Name, USkeletalMeshComponent* Component)
	{
		USkeletalMesh* Mesh = Component ? Component->GetSkeletalMeshAsset() : nullptr;
		FTransform HeadLocal = FTransform::Identity;
		FTransform HeadComponent = FTransform::Identity;
		int32 HeadIndex = INDEX_NONE;
		if (Mesh)
		{
			const FReferenceSkeleton& ReferenceSkeleton = Mesh->GetRefSkeleton();
			HeadIndex = ReferenceSkeleton.FindBoneIndex(TEXT("J_Bip_C_Head"));
			if (HeadIndex != INDEX_NONE)
			{
				HeadLocal = ReferenceSkeleton.GetRefBonePose()[HeadIndex];
				HeadComponent = HeadLocal;
				for (int32 ParentIndex = ReferenceSkeleton.GetParentIndex(HeadIndex);
					ParentIndex != INDEX_NONE;
					ParentIndex = ReferenceSkeleton.GetParentIndex(ParentIndex))
				{
					HeadComponent *= ReferenceSkeleton.GetRefBonePose()[ParentIndex];
				}
			}
		}
		UE_LOG(LogTemp, Display, TEXT("VRoid QA part %s mesh=%s visible=%d materials=%d bounds=%s headIndex=%d headLocal=%s headComponent=%s"),
			Name,
			*GetPathNameSafe(Mesh),
			Component && Component->IsVisible(),
			Component ? Component->GetNumMaterials() : 0,
			Component ? *Component->Bounds.GetBox().ToString() : TEXT("none"),
			HeadIndex,
			*HeadLocal.ToHumanReadableString(),
			*HeadComponent.ToHumanReadableString());
	};
	LogPart(TEXT("FaceSkin"), FaceSkinMesh);
	LogPart(TEXT("EyeWhite"), EyeWhiteMesh);
	LogPart(TEXT("EyeIris"), EyeIrisMesh);
	LogPart(TEXT("EyeHighlight"), EyeHighlightMesh);
	LogPart(TEXT("Eyelash"), EyelashMesh);
	LogPart(TEXT("Eyeline"), EyelineMesh);
	LogPart(TEXT("HairScalp"), HairScalpMesh);
	LogPart(TEXT("HairBase"), HairBaseMesh);
	LogPart(TEXT("HairFront"), HairFrontMesh);
	LogPart(TEXT("HairSide"), HairSideMesh);
	LogPart(TEXT("HairBack"), HairBackMesh);
	LogPart(TEXT("HairExtra"), HairExtraMesh);
	LogPart(TEXT("Top"), TopMesh);
	LogPart(TEXT("Bottom"), BottomMesh);
	LogPart(TEXT("Onepiece"), OnepieceMesh);
	LogPart(TEXT("Shoes"), ShoesMesh);
	LogPart(TEXT("HeadAccessory"), HeadAccessoryMesh);
	LogPart(TEXT("FaceAccessory"), FaceAccessoryMesh);
	LogPart(TEXT("EarAccessory"), EarAccessoryMesh);
	LogPart(TEXT("TailAccessory"), TailAccessoryMesh);
	LogPart(TEXT("NeckAccessory"), NeckAccessoryMesh);
	UE_LOG(LogTemp, Display,
		TEXT("VRoid QA selection gender=%s hair=%d/%d/%d/%d outfit=%d/%d/%d/%d accessories=%d/%d/%d/%d/%d"),
		Appearance.Gender == EUECharacterGender::Female ? TEXT("Female") : TEXT("Male"),
		Appearance.HairFrontStyle, Appearance.HairSideStyle, Appearance.HairBackStyle, Appearance.HairExtraStyle,
		Appearance.TopStyle, Appearance.BottomStyle, Appearance.OnepieceStyle, Appearance.ShoesStyle,
		Appearance.HeadAccessoryStyle, Appearance.FaceAccessoryStyle, Appearance.EarAccessoryStyle,
		Appearance.TailAccessoryStyle, Appearance.NeckAccessoryStyle);
	auto LogFaceMaterial = [](const TCHAR* Name, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials)
	{
		if (Materials.IsEmpty() || !Materials[0])
		{
			UE_LOG(LogTemp, Display, TEXT("VRoid QA material %s missing"), Name);
			return;
		}
		UMaterialInstanceDynamic* Material = Materials[0];
		UMaterial* ResolvedMaterial = Material->GetMaterial();
		UTexture* DiffuseTexture = Material->K2_GetTextureParameterValue(TEXT("DiffuseColorMap"));
		UTexture* OpacityTexture = Material->K2_GetTextureParameterValue(TEXT("OpacityMaskMap"));
		UE_LOG(LogTemp, Display,
			TEXT("VRoid QA material %s resolved=%s blend=%d diffuse=%s emissive=%s tint=%s original=%s custom=%s iris=%s eyeWhite=%s diffuseTexture=%s opacityTexture=%s emissionWeight=%.3f opacityMaskWeight=%.3f twoSided=%d"),
			Name,
			*GetPathNameSafe(ResolvedMaterial),
			ResolvedMaterial ? static_cast<int32>(ResolvedMaterial->GetBlendMode()) : -1,
			*Material->K2_GetVectorParameterValue(TEXT("DiffuseColor")).ToString(),
			*Material->K2_GetVectorParameterValue(TEXT("EmissiveColor")).ToString(),
			*Material->K2_GetVectorParameterValue(TEXT("TintColor")).ToString(),
			*Material->K2_GetVectorParameterValue(TEXT("OriginalTint")).ToString(),
			*Material->K2_GetVectorParameterValue(TEXT("CustomTint")).ToString(),
			*Material->K2_GetVectorParameterValue(TEXT("IrisColor")).ToString(),
			*Material->K2_GetVectorParameterValue(TEXT("EyeWhiteTint")).ToString(),
			*GetPathNameSafe(DiffuseTexture),
			*GetPathNameSafe(OpacityTexture),
			Material->K2_GetScalarParameterValue(TEXT("EmissiveColorMapWeight")),
			Material->K2_GetScalarParameterValue(TEXT("OpacityMaskMapWeight")),
			Material->IsTwoSided());
	};
	LogFaceMaterial(TEXT("EyeWhite"), EyeWhiteMaterials);
	LogFaceMaterial(TEXT("BodySkin"), BodySkinMaterials);
	LogFaceMaterial(TEXT("FaceSkin"), FaceSkinMaterials);
	LogFaceMaterial(TEXT("Iris"), EyeMaterials);
	LogFaceMaterial(TEXT("Brow"), BrowMaterials);
	LogFaceMaterial(TEXT("Eyelash"), EyelashMaterials);
	LogFaceMaterial(TEXT("Eyeline"), EyelineMaterials);
	LogFaceMaterial(TEXT("Mouth"), MouthMaterials);
	LogFaceMaterial(TEXT("Top"), TopMaterials);
	LogFaceMaterial(TEXT("Bottom"), BottomMaterials);
	LogFaceMaterial(TEXT("Onepiece"), OnepieceMaterials);
	LogFaceMaterial(TEXT("Shoes"), ShoesMaterials);
	FString OnlyFacePart;
	if (FParse::Value(FCommandLine::Get(), TEXT("VRoidQAOnlyFacePart="), OnlyFacePart))
	{
		const TArray<TPair<FString, USkeletalMeshComponent*>> FaceParts = {
			{TEXT("EyeWhite"), EyeWhiteMesh},
			{TEXT("Iris"), EyeIrisMesh},
			{TEXT("Highlight"), EyeHighlightMesh},
			{TEXT("EyeExtra"), EyeExtraMesh},
			{TEXT("Brow"), BrowMesh},
			{TEXT("Eyelash"), EyelashMesh},
			{TEXT("Eyeline"), EyelineMesh},
			{TEXT("Mouth"), MouthMesh},
			{TEXT("Lip"), LipOverlayMesh},
			{TEXT("MouthLine"), MouthLineOverlayMesh}};
		for (const TPair<FString, USkeletalMeshComponent*>& Entry : FaceParts)
		{
			Entry.Value->SetVisibility(Entry.Key.Equals(OnlyFacePart, ESearchCase::IgnoreCase), true);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("VRoidQANoHair")))
	{
		HairScalpMesh->SetVisibility(false, true);
		HairBaseMesh->SetVisibility(false, true);
		HairFrontMesh->SetVisibility(false, true);
		HairSideMesh->SetVisibility(false, true);
		HairBackMesh->SetVisibility(false, true);
		HairExtraMesh->SetVisibility(false, true);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("VRoidQANoFaceDetails")))
	{
		EyeWhiteMesh->SetVisibility(false, true);
		EyeIrisMesh->SetVisibility(false, true);
		EyeHighlightMesh->SetVisibility(false, true);
		EyeExtraMesh->SetVisibility(false, true);
		BrowMesh->SetVisibility(false, true);
		EyelashMesh->SetVisibility(false, true);
		EyelineMesh->SetVisibility(false, true);
		MouthMesh->SetVisibility(false, true);
		LipOverlayMesh->SetVisibility(false, true);
		MouthLineOverlayMesh->SetVisibility(false, true);
	}
	const TCHAR* GenderName = Appearance.Gender == EUECharacterGender::Female ? TEXT("Female") : TEXT("Male");
	const TCHAR* StyleName = FParse::Param(FCommandLine::Get(), TEXT("VRoidQASecondStyle")) ? TEXT("_Style2") : TEXT("");
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		FString::Printf(TEXT("Screenshots/Customization/VRoidCustomization_%s%s_Full.png"), GenderName, StyleName);
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	GetWorldTimerManager().SetTimer(QAPrepareHeadTimer, this, &ThisClass::PrepareQAHeadScreenshot, 0.75f, false);
}

void AUECustomizationPreviewActor::PrepareQAHeadScreenshot()
{
	const FVector HeadCenter = FaceSkinMesh
		? SceneRoot->GetComponentTransform().InverseTransformPosition(FaceSkinMesh->Bounds.Origin)
		: FVector(0.0f, 0.0f, 152.0f);
	PreviewCamera->SetRelativeLocation(FVector(130.0f, 0.0f, HeadCenter.Z));
	PreviewCamera->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	PreviewCamera->FieldOfView = 28.0f;
	GetWorldTimerManager().SetTimer(QAHeadScreenshotTimer, this, &ThisClass::CaptureQAHeadScreenshot, 1.0f, false);
}

void AUECustomizationPreviewActor::CaptureQAHeadScreenshot()
{
	const TCHAR* GenderName = Appearance.Gender == EUECharacterGender::Female ? TEXT("Female") : TEXT("Male");
	const TCHAR* StyleName = FParse::Param(FCommandLine::Get(), TEXT("VRoidQASecondStyle")) ? TEXT("_Style2") : TEXT("");
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		FString::Printf(TEXT("Screenshots/Customization/VRoidCustomization_%s%s_Head.png"), GenderName, StyleName);
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	GetWorldTimerManager().SetTimer(QAExitTimer, this, &ThisClass::ExitAfterQAScreenshot, 1.0f, false);
}

void AUECustomizationPreviewActor::ExitAfterQAScreenshot()
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

void AUECustomizationPreviewActor::SetMaterialColor(
	const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials,
	const FName& Parameter,
	const FLinearColor& Color)
{
	for (UMaterialInstanceDynamic* Material : Materials)
	{
		if (Material)
		{
			Material->SetVectorParameterValue(Parameter, Color);
		}
	}
}

void AUECustomizationPreviewActor::SetMaterialScalar(
	const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials,
	const FName& Parameter,
	float Value)
{
	for (UMaterialInstanceDynamic* Material : Materials)
	{
		if (Material)
		{
			Material->SetScalarParameterValue(Parameter, Value);
		}
	}
}
