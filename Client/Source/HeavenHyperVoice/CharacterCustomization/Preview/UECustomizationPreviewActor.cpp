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
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

const FString AUECustomizationPreviewActor::SaveSlotName(TEXT("UECharacterAppearance"));

namespace
{
	const FName DiffuseColorParameter(TEXT("DiffuseColor"));
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
	default: return UE_SELECT_CATALOG(Body);
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
		const int32 Count = GetOptionCount(Part);
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
	if (Part == EUECustomizationPart::Gender)
	{
		return 2;
	}
	if (Part == EUECustomizationPart::Body)
	{
		return 3;
	}
	if (Part == EUECustomizationPart::HairSet)
	{
		return FMath::Min(
			GetCatalog(EUECustomizationPart::HairFront).Num(),
			FMath::Min(GetCatalog(EUECustomizationPart::HairBack).Num(), GetCatalog(EUECustomizationPart::HairSide).Num()));
	}
	const TArray<TObjectPtr<UTexture2D>>& Textures = GetTextureCatalog(Part);
	if (!Textures.IsEmpty())
	{
		return Textures.Num();
	}
	return GetCatalog(Part).Num();
}

FString AUECustomizationPreviewActor::GetOptionLabel(EUECustomizationPart Part, int32 Index) const
{
	if (Part == EUECustomizationPart::Gender)
	{
		return Index == 0 ? TEXT("Male") : TEXT("Female");
	}
	if (Part == EUECustomizationPart::Body)
	{
		static const TCHAR* Names[] = {TEXT("Slim"), TEXT("Standard"), TEXT("Wide")};
		return Names[FMath::Clamp(Index, 0, 2)];
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
	if (!GetTextureCatalog(Part).IsEmpty() && GetTextureCatalog(Part).IsValidIndex(Index) &&
		GetTextureCatalog(Part)[Index] == nullptr)
	{
		return TEXT("None");
	}
	if (Part == EUECustomizationPart::EyeExtra && Index == 0)
	{
		return TEXT("None");
	}
	if (Part == EUECustomizationPart::HairBase)
	{
		static const TCHAR* Names[] = {TEXT("None"), TEXT("Default"), TEXT("Preset 001"), TEXT("Initial")};
		return Names[FMath::Clamp(Index, 0, 3)];
	}
	if (Part == EUECustomizationPart::HairExtra &&
		GetCatalog(Part).IsValidIndex(Index) && GetCatalog(Part)[Index] == nullptr)
	{
		return TEXT("None");
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
		return StyleIds.IsValidIndex(Index)
			? FString::Printf(TEXT("Hair %d"), StyleIds[Index])
			: TEXT("Hair");
	}
	const TArray<TObjectPtr<USkeletalMesh>>& MeshCatalog = GetCatalog(Part);
	if (MeshCatalog.IsValidIndex(Index) && MeshCatalog[Index] == nullptr)
	{
		return TEXT("None");
	}
	if (MeshCatalog.IsValidIndex(Index) && MeshCatalog[Index])
	{
		FString Label = MeshCatalog[Index]->GetName();
		Label.RemoveFromStart(TEXT("SK_"));
		Label.ReplaceInline(TEXT("_Male_"), TEXT(" "));
		Label.ReplaceInline(TEXT("_Female_"), TEXT(" "));
		Label.ReplaceInline(TEXT("_"), TEXT(" "));
		return Label;
	}
	return FString::Printf(TEXT("Style %02d"), Index + 1);
}

UTexture2D* AUECustomizationPreviewActor::GetOptionTexture(EUECustomizationPart Part, int32 Index) const
{
	const TArray<TObjectPtr<UTexture2D>>& Catalog = GetTextureCatalog(Part);
	return Catalog.IsValidIndex(Index) ? Catalog[Index].Get() : nullptr;
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
	Assign(HairScalpMesh, SelectMesh(GetCatalog(EUECustomizationPart::HairBase), Appearance.HairBaseStyle));
	Assign(HairBaseMesh, SelectMesh(GetCatalog(EUECustomizationPart::HairBase), Appearance.HairBaseStyle));
	Assign(HairFrontMesh, SelectMesh(GetCatalog(EUECustomizationPart::HairFront), Appearance.HairFrontStyle));
	Assign(HairSideMesh, SelectMesh(GetCatalog(EUECustomizationPart::HairSide), Appearance.HairSideStyle));
	Assign(HairBackMesh, SelectMesh(GetCatalog(EUECustomizationPart::HairBack), Appearance.HairBackStyle));
	Assign(HairExtraMesh, SelectMesh(GetCatalog(EUECustomizationPart::HairExtra), Appearance.HairExtraStyle));
	Assign(TopMesh, SelectMesh(GetCatalog(EUECustomizationPart::Top), Appearance.TopStyle));
	Assign(BottomMesh, SelectMesh(GetCatalog(EUECustomizationPart::Bottom), Appearance.BottomStyle));
	Assign(OnepieceMesh, SelectMesh(GetCatalog(EUECustomizationPart::Onepiece), Appearance.OnepieceStyle));
	Assign(ShoesMesh, SelectMesh(GetCatalog(EUECustomizationPart::Shoes), Appearance.ShoesStyle));
	Assign(HeadAccessoryMesh, SelectMesh(GetCatalog(EUECustomizationPart::HeadAccessory), Appearance.HeadAccessoryStyle));
	Assign(FaceAccessoryMesh, SelectMesh(GetCatalog(EUECustomizationPart::FaceAccessory), Appearance.FaceAccessoryStyle));
	Assign(EarAccessoryMesh, SelectMesh(GetCatalog(EUECustomizationPart::EarAccessory), Appearance.EarAccessoryStyle));
	Assign(TailAccessoryMesh, SelectMesh(GetCatalog(EUECustomizationPart::TailAccessory), Appearance.TailAccessoryStyle));
	Assign(NeckAccessoryMesh, SelectMesh(GetCatalog(EUECustomizationPart::NeckAccessory), Appearance.NeckAccessoryStyle));

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
	if (FParse::Param(FCommandLine::Get(), TEXT("VRoidQAReset")))
	{
		Appearance = FUECharacterCustomizationData();
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
	Appearance.Normalize();
	ClampAppearanceToCatalogs();
	if (UpdateMeshes() || BodySkinMaterials.IsEmpty())
	{
		CreateDynamicMaterials();
	}
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
	ApplyAppearance(FUECharacterCustomizationData());
}

void AUECustomizationPreviewActor::RandomizeAppearance()
{
	FUECharacterCustomizationData RandomAppearance;
	RandomAppearance.Gender = FMath::RandBool() ? EUECharacterGender::Female : EUECharacterGender::Male;
	RandomAppearance.BodyPreset = static_cast<EUEBodyPreset>(FMath::RandRange(0, 2));
	RandomAppearance.Height = FMath::FRand();
	RandomAppearance.HeadSize = FMath::FRand();
	RandomAppearance.ShoulderWidth = FMath::FRand();

	const TArray<FLinearColor> SkinColors = {
		FLinearColor::FromSRGBColor(FColor(239, 185, 151)),
		FLinearColor::FromSRGBColor(FColor(184, 128, 92)),
		FLinearColor::FromSRGBColor(FColor(105, 66, 47))};
	const TArray<FLinearColor> HairTints = {
		FLinearColor::White, FLinearColor(0.12f, 0.22f, 0.55f), FLinearColor(0.55f, 0.12f, 0.16f)};
	const TArray<FLinearColor> EyeTints = {
		FLinearColor(0.38f, 0.16f, 0.06f),
		FLinearColor(0.12f, 0.38f, 0.72f),
		FLinearColor(0.16f, 0.42f, 0.22f)};
	const TArray<FLinearColor> LipTints = {
		FLinearColor::FromSRGBColor(FColor(196, 102, 116)),
		FLinearColor::FromSRGBColor(FColor(225, 154, 165)),
		FLinearColor::FromSRGBColor(FColor(133, 52, 70))};
	const TArray<FLinearColor> OutfitTints = {
		FLinearColor::White, FLinearColor(0.25f, 0.65f, 1.0f), FLinearColor(1.0f, 0.55f, 0.25f)};

	RandomAppearance.SkinColor = SkinColors[FMath::RandRange(0, SkinColors.Num() - 1)];
	RandomAppearance.HairColor = HairTints[FMath::RandRange(0, HairTints.Num() - 1)];
	RandomAppearance.EyeColor = EyeTints[FMath::RandRange(0, EyeTints.Num() - 1)];
	RandomAppearance.LipColor = LipTints[FMath::RandRange(0, LipTints.Num() - 1)];
	RandomAppearance.OutfitColor = OutfitTints[FMath::RandRange(0, OutfitTints.Num() - 1)];
	RandomAppearance.TopColor = OutfitTints[FMath::RandRange(0, OutfitTints.Num() - 1)];
	RandomAppearance.BottomColor = OutfitTints[FMath::RandRange(0, OutfitTints.Num() - 1)];
	RandomAppearance.OnepieceColor = OutfitTints[FMath::RandRange(0, OutfitTints.Num() - 1)];
	RandomAppearance.ShoesColor = OutfitTints[FMath::RandRange(0, OutfitTints.Num() - 1)];
	RandomAppearance.AccessoryColor = OutfitTints[FMath::RandRange(0, OutfitTints.Num() - 1)];
	auto RandomIndex = [this](EUECustomizationPart Part)
	{
		return FMath::RandRange(0, FMath::Max(GetOptionCount(Part) - 1, 0));
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
	RandomAppearance.HairBaseStyle = FMath::Max(1, RandomIndex(EUECustomizationPart::HairBase));
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
	SaveGame->DataVersion = 10;
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
		Appearance.EyeColor = FLinearColor::White;
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
		if (UMaterialInterface* Parent = TemplateMaterial ? TemplateMaterial : Component->GetMaterial(Index))
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this);
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
		Material->SetScalarParameterValue(TEXT("OpacityMask"), 1.0f);
		Material->SetScalarParameterValue(TEXT("OpacityMaskMapWeight"), 1.0f);
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

	CreateMaterialsForComponent(BodyMesh, BodySkinMaterials);
	CreateOpaqueFaceMaterials(
		FaceSkinMesh,
		FaceSkinMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/Skin.Skin")));
	CreateMaskedOverlayMaterials(
		EyeIrisMesh,
		EyeMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/EyeIris.EyeIris")));
	CreateMaskedOverlayMaterials(
		EyeWhiteMesh,
		EyeWhiteMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/EyeWhite.EyeWhite")));
	CreateMaskedOverlayMaterials(
		EyeHighlightMesh,
		EyeHighlightMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/EyeHighlight.EyeHighlight")));
	CreateMaskedOverlayMaterials(
		EyeExtraMesh,
		EyeExtraMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/EyeExtra.EyeExtra")));
	CreateMaskedOverlayMaterials(
		BrowMesh,
		BrowMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/Brow.Brow")));
	CreateMaskedOverlayMaterials(
		EyelashMesh,
		EyelashMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/Eyelash.Eyelash")));
	CreateMaskedOverlayMaterials(
		EyelineMesh,
		EyelineMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/EyelineOverlay.EyelineOverlay")));
	CreateMaskedOverlayMaterials(
		MouthMesh,
		MouthMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/Mouth.Mouth")));
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
	CreateMaterialsForComponent(HairScalpMesh, HairScalpMaterials);
	HairMaterials.Append(HairScalpMaterials);
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
	CreateMaterialsForComponent(
		FaceAccessoryMesh,
		FaceAccessoryMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/CharacterCustomization/Materials/M_UEFaceAccessoryTranslucent.M_UEFaceAccessoryTranslucent")));
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
		Material->SetScalarParameterValue(TEXT("OpacityMaskMapWeight"), 0.0f);
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
	auto FlattenMaskedLayer = [](
		const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials)
	{
		SetMaterialScalar(Materials, TEXT("DiffuseColorMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("EmissiveColorMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("SpecularColorMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("Shininess"), 0.0f);
	};
	auto ApplyTexturedTint = [](const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, const FLinearColor& Color)
	{
		SetMaterialColor(Materials, DiffuseColorParameter, Color);
		SetMaterialColor(Materials, TEXT("BaseColor"), Color);
		SetMaterialColor(Materials, TEXT("Base Color"), Color);
		SetMaterialColor(Materials, TEXT("AmbientColor"), Color);
		SetMaterialColor(Materials, TEXT("TintColor"), Color);
		SetMaterialColor(Materials, TEXT("Color"), Color);
		SetMaterialColor(Materials, TEXT("EmissiveColor"), Color * 0.18f);
		SetMaterialColor(Materials, TEXT("SpecularColor"), FLinearColor::Black);
		SetMaterialScalar(Materials, TEXT("DiffuseColorMapWeight"), 1.0f);
		SetMaterialScalar(Materials, TEXT("Opacity"), 1.0f);
		SetMaterialScalar(Materials, TEXT("OpacityMask"), 1.0f);
		SetMaterialScalar(Materials, TEXT("OpacityMaskMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("SpecularColorMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("Shininess"), 0.0f);
	};
	auto ApplyAccessoryTint = [](const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, const FLinearColor& Color)
	{
		SetMaterialColor(Materials, DiffuseColorParameter, Color);
		SetMaterialColor(Materials, TEXT("BaseColor"), Color);
		SetMaterialColor(Materials, TEXT("Base Color"), Color);
		SetMaterialColor(Materials, TEXT("AmbientColor"), Color);
		SetMaterialColor(Materials, TEXT("TintColor"), Color);
		SetMaterialColor(Materials, TEXT("Color"), Color);
		SetMaterialColor(Materials, TEXT("AccessoryColor"), Color);
		SetMaterialColor(Materials, TEXT("EmissiveColor"), Color * 0.08f);
		SetMaterialColor(Materials, TEXT("SpecularColor"), FLinearColor::Black);
		SetMaterialScalar(Materials, TEXT("DiffuseColorMapWeight"), 1.0f);
		SetMaterialScalar(Materials, TEXT("SpecularColorMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("Shininess"), 0.0f);
	};
	const FLinearColor SkinColor = Appearance.SkinColor;
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
	SetMaterialColor(BodySkinMaterials, DiffuseColorParameter, SkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("BaseColor"), SkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("Base Color"), SkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("AmbientColor"), SkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("EmissiveColor"), SkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("TintColor"), SkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("Color"), SkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	SetMaterialScalar(BodySkinMaterials, TEXT("DiffuseColorMapWeight"), 1.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("Opacity"), 1.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("OpacityMaskMapWeight"), 0.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("EmissiveColorMapWeight"), 0.2f);
	SetMaterialScalar(BodySkinMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("Shininess"), 0.0f);
	SetMaterialColor(FaceSkinMaterials, DiffuseColorParameter, SkinColor);
	SetMaterialColor(FaceSkinMaterials, TEXT("BaseColor"), SkinColor);
	SetMaterialColor(FaceSkinMaterials, TEXT("Base Color"), SkinColor);
	SetMaterialColor(FaceSkinMaterials, TEXT("AmbientColor"), SkinColor);
	SetMaterialColor(FaceSkinMaterials, TEXT("EmissiveColor"), SkinColor);
	SetMaterialColor(FaceSkinMaterials, TEXT("TintColor"), SkinColor);
	SetMaterialColor(FaceSkinMaterials, TEXT("Color"), SkinColor);
	SetMaterialColor(FaceSkinMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	SetMaterialScalar(FaceSkinMaterials, TEXT("DiffuseColorMapWeight"), 1.0f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("Opacity"), 1.0f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("OpacityMaskMapWeight"), 0.0f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("EmissiveColorMapWeight"), 0.2f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(FaceSkinMaterials, TEXT("Shininess"), 0.0f);
	SetMaterialColor(HairMaterials, DiffuseColorParameter, Appearance.HairColor);
	SetMaterialColor(HairMaterials, TEXT("BaseColor"), Appearance.HairColor);
	SetMaterialColor(HairMaterials, TEXT("Base Color"), Appearance.HairColor);
	SetMaterialColor(HairMaterials, TEXT("AmbientColor"), Appearance.HairColor);
	SetMaterialColor(HairMaterials, TEXT("TintColor"), Appearance.HairColor);
	SetMaterialColor(HairMaterials, TEXT("Color"), Appearance.HairColor);
	SetMaterialColor(HairMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	SetMaterialScalar(HairMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(HairMaterials, TEXT("Shininess"), 0.0f);
	SetMaterialScalar(HairScalpMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(HairScalpMaterials, TEXT("OpacityMaskMapWeight"), 0.0f);
	SetMaterialColor(EyeMaterials, DiffuseColorParameter, Appearance.EyeColor);
	SetMaterialColor(EyeMaterials, TEXT("BaseColor"), Appearance.EyeColor);
	SetMaterialColor(EyeMaterials, TEXT("Base Color"), Appearance.EyeColor);
	SetMaterialColor(EyeMaterials, TEXT("AmbientColor"), Appearance.EyeColor);
	SetMaterialColor(EyeMaterials, TEXT("EmissiveColor"), Appearance.EyeColor);
	SetMaterialColor(EyeMaterials, TEXT("TintColor"), Appearance.EyeColor);
	SetMaterialColor(EyeMaterials, TEXT("Color"), Appearance.EyeColor);
	SetMaterialColor(EyeMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	SetMaterialScalar(EyeMaterials, TEXT("DiffuseColorMapWeight"), 0.68f);
	SetMaterialScalar(EyeMaterials, TEXT("EmissiveColorMapWeight"), 0.32f);
	SetMaterialColor(EyeWhiteMaterials, DiffuseColorParameter, FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("BaseColor"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("Base Color"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("AmbientColor"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("EmissiveColor"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("TintColor"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("Color"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("DiffuseColorMapWeight"), 1.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("OpacityMaskMapWeight"), 1.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("EmissiveColorMapWeight"), 0.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("Shininess"), 0.0f);
	SetMaterialColor(EyeHighlightMaterials, DiffuseColorParameter, FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("BaseColor"), FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("Base Color"), FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("AmbientColor"), FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("EmissiveColor"), FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("TintColor"), FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("Color"), FLinearColor::White);
	SetMaterialColor(EyeExtraMaterials, DiffuseColorParameter, Appearance.EyeColor);
	SetMaterialColor(EyeExtraMaterials, TEXT("BaseColor"), Appearance.EyeColor);
	SetMaterialColor(EyeExtraMaterials, TEXT("Base Color"), Appearance.EyeColor);
	SetMaterialColor(EyeExtraMaterials, TEXT("AmbientColor"), Appearance.EyeColor);
	SetMaterialColor(EyeExtraMaterials, TEXT("EmissiveColor"), Appearance.EyeColor);
	SetMaterialColor(EyeExtraMaterials, TEXT("TintColor"), Appearance.EyeColor);
	SetMaterialColor(EyeExtraMaterials, TEXT("Color"), Appearance.EyeColor);
	SetMaterialColor(BrowMaterials, DiffuseColorParameter, BrowColor);
	SetMaterialColor(BrowMaterials, TEXT("BaseColor"), BrowColor);
	SetMaterialColor(BrowMaterials, TEXT("AmbientColor"), BrowColor);
	SetMaterialColor(BrowMaterials, TEXT("EmissiveColor"), BrowColor);
	SetMaterialColor(BrowMaterials, TEXT("TintColor"), BrowColor);
	SetMaterialColor(BrowMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	FlattenMaskedLayer(BrowMaterials);
	SetMaterialColor(EyelashMaterials, DiffuseColorParameter, LashColor);
	SetMaterialColor(EyelashMaterials, TEXT("BaseColor"), LashColor);
	SetMaterialColor(EyelashMaterials, TEXT("AmbientColor"), LashColor);
	SetMaterialColor(EyelashMaterials, TEXT("EmissiveColor"), LashColor);
	SetMaterialColor(EyelashMaterials, TEXT("TintColor"), LashColor);
	SetMaterialColor(EyelashMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	FlattenMaskedLayer(EyelashMaterials);
	SetMaterialColor(EyelineMaterials, DiffuseColorParameter, LashColor);
	SetMaterialColor(EyelineMaterials, TEXT("BaseColor"), LashColor);
	SetMaterialColor(EyelineMaterials, TEXT("AmbientColor"), LashColor);
	SetMaterialColor(EyelineMaterials, TEXT("EmissiveColor"), LashColor);
	SetMaterialColor(EyelineMaterials, TEXT("TintColor"), LashColor);
	SetMaterialColor(EyelineMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	FlattenMaskedLayer(EyelineMaterials);
	SetMaterialColor(MouthMaterials, DiffuseColorParameter, FLinearColor::White);
	SetMaterialColor(MouthMaterials, TEXT("BaseColor"), FLinearColor::White);
	SetMaterialColor(MouthMaterials, TEXT("AmbientColor"), FLinearColor::White);
	SetMaterialColor(MouthMaterials, TEXT("EmissiveColor"), FLinearColor::White);
	SetMaterialColor(MouthMaterials, TEXT("TintColor"), FLinearColor::White);
	SetMaterialScalar(MouthMaterials, TEXT("Opacity"), 1.0f);
	SetMaterialScalar(MouthMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(MouthMaterials, TEXT("OpacityMaskMapWeight"), 0.0f);
	SetMaterialColor(LipMaterials, DiffuseColorParameter, Appearance.LipColor);
	SetMaterialColor(LipMaterials, TEXT("BaseColor"), Appearance.LipColor);
	SetMaterialColor(LipMaterials, TEXT("AmbientColor"), Appearance.LipColor);
	SetMaterialColor(LipMaterials, TEXT("EmissiveColor"), Appearance.LipColor);
	SetMaterialColor(LipMaterials, TEXT("TintColor"), Appearance.LipColor);
	SetMaterialColor(LipMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	SetMaterialScalar(LipMaterials, TEXT("DiffuseColorMapWeight"), 0.0f);
	SetMaterialScalar(LipMaterials, TEXT("EmissiveColorMapWeight"), 0.0f);
	SetMaterialScalar(LipMaterials, TEXT("OpacityMaskMapWeight"), 1.0f);
	SetMaterialScalar(LipMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(LipMaterials, TEXT("Shininess"), 0.0f);
	const FLinearColor MouthLineColor(
		Appearance.LipColor.R * 0.55f,
		Appearance.LipColor.G * 0.55f,
		Appearance.LipColor.B * 0.55f,
		Appearance.LipColor.A);
	SetMaterialColor(MouthLineMaterials, DiffuseColorParameter, MouthLineColor);
	SetMaterialColor(MouthLineMaterials, TEXT("BaseColor"), MouthLineColor);
	SetMaterialColor(MouthLineMaterials, TEXT("AmbientColor"), MouthLineColor);
	SetMaterialColor(MouthLineMaterials, TEXT("EmissiveColor"), MouthLineColor);
	SetMaterialColor(MouthLineMaterials, TEXT("TintColor"), MouthLineColor);
	SetMaterialColor(MouthLineMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	SetMaterialScalar(MouthLineMaterials, TEXT("DiffuseColorMapWeight"), 0.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("EmissiveColorMapWeight"), 0.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("OpacityMaskMapWeight"), 1.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("Shininess"), 0.0f);
	ApplyTexturedTint(TopMaterials, Appearance.TopColor);
	ApplyTexturedTint(BottomMaterials, Appearance.BottomColor);
	ApplyTexturedTint(OnepieceMaterials, Appearance.OnepieceColor);
	ApplyTexturedTint(ShoesMaterials, Appearance.ShoesColor);
	ApplyAccessoryTint(AccessoryMaterials, Appearance.AccessoryColor);
	SetMaterialScalar(FaceAccessoryMaterials, TEXT("Opacity"), 0.12f);
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

	ApplyTextureEmission(BodySkinMaterials, 1.35f);
	ApplyTextureEmission(FaceSkinMaterials, 1.35f);
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

	const float FaceLayerDepth = FMath::Max(CatalogAsset ? CatalogAsset->ScleraDepthOffsetY : 0.005f, 0.04f);
	auto OffsetFaceLayer = [FaceLayerDepth](USkeletalMeshComponent* Component, float Order)
	{
		if (Component)
		{
			Component->AddLocalOffset(FVector(0.0f, FaceLayerDepth * Order, 0.0f));
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

	const float HeadAccessoryOffset = CatalogAsset ? CatalogAsset->HeadAccessoryVerticalOffset : 9.0f;
	const float FaceForwardOffset = CatalogAsset ? CatalogAsset->FaceAccessoryForwardOffset : 6.5f;
	const float FaceVerticalOffset = 12.5f;
	if (HeadAccessoryMesh)
	{
		HeadAccessoryMesh->SetRelativeLocation(FVector(0.0f, 0.0f, HeadAccessoryOffset));
	}
	if (FaceAccessoryMesh)
	{
		FaceAccessoryMesh->SetRelativeLocation(FVector(0.0f, FaceForwardOffset, FaceVerticalOffset));
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
		UE_LOG(LogTemp, Display,
			TEXT("VRoid QA material %s diffuse=%s emissive=%s tint=%s emissionWeight=%.3f twoSided=%d"),
			Name,
			*Material->K2_GetVectorParameterValue(TEXT("DiffuseColor")).ToString(),
			*Material->K2_GetVectorParameterValue(TEXT("EmissiveColor")).ToString(),
			*Material->K2_GetVectorParameterValue(TEXT("TintColor")).ToString(),
			Material->K2_GetScalarParameterValue(TEXT("EmissiveColorMapWeight")),
			Material->IsTwoSided());
	};
	LogFaceMaterial(TEXT("EyeWhite"), EyeWhiteMaterials);
	LogFaceMaterial(TEXT("FaceSkin"), FaceSkinMaterials);
	LogFaceMaterial(TEXT("Iris"), EyeMaterials);
	LogFaceMaterial(TEXT("Brow"), BrowMaterials);
	LogFaceMaterial(TEXT("Eyelash"), EyelashMaterials);
	LogFaceMaterial(TEXT("Eyeline"), EyelineMaterials);
	LogFaceMaterial(TEXT("Mouth"), MouthMaterials);
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
