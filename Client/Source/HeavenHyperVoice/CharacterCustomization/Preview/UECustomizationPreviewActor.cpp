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

	FLinearColor MakeFaceSkinTint(const FLinearColor& Target, const FLinearColor& Reference)
	{
		return FLinearColor(
			FMath::Clamp(Target.R / FMath::Max(Reference.R, UE_SMALL_NUMBER), 0.0f, 4.0f),
			FMath::Clamp(Target.G / FMath::Max(Reference.G, UE_SMALL_NUMBER), 0.0f, 4.0f),
			FMath::Clamp(Target.B / FMath::Max(Reference.B, UE_SMALL_NUMBER), 0.0f, 4.0f),
			1.0f);
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
	Part->SetCastShadow(true);
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
	bool bChanged = false;
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
	Assign(FaceSkinMesh, SelectMesh(GetCatalog(EUECustomizationPart::FaceSkin), 0));
	Assign(EyeWhiteMesh, SelectMesh(GetCatalog(EUECustomizationPart::EyeWhite), 0));
	Assign(EyeIrisMesh, SelectMesh(GetCatalog(EUECustomizationPart::EyeIris), 0));
	Assign(EyeHighlightMesh, SelectMesh(GetCatalog(EUECustomizationPart::EyeHighlight), 0));
	Assign(EyeExtraMesh, SelectMesh(GetCatalog(EUECustomizationPart::EyeExtra), Appearance.EyeExtraStyle));
	Assign(BrowMesh, SelectMesh(GetCatalog(EUECustomizationPart::Brow), 0));
	Assign(EyelashMesh, SelectMesh(GetCatalog(EUECustomizationPart::Eyelash), 0));
	Assign(EyelineMesh, SelectMesh(GetCatalog(EUECustomizationPart::Eyeline), 0));
	Assign(MouthMesh, SelectMesh(GetCatalog(EUECustomizationPart::Mouth), 0));
	Assign(LipOverlayMesh, SelectMesh(GetCatalog(EUECustomizationPart::FaceSkin), 0));
	Assign(MouthLineOverlayMesh, SelectMesh(GetCatalog(EUECustomizationPart::FaceSkin), 0));
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
		TopMesh, BottomMesh, OnepieceMesh, ShoesMesh,
		HeadAccessoryMesh, FaceAccessoryMesh, EarAccessoryMesh, TailAccessoryMesh, NeckAccessoryMesh};
	for (USkeletalMeshComponent* Follower : Followers)
	{
		Follower->SetLeaderPoseComponent(BodyMesh);
	}
	FaceAccessoryMesh->SetLeaderPoseComponent(nullptr);

	// Hair FBX files use their own VRoid head bind pose. Copying the body's pose
	// applies a different inverse bind transform and folds the strands into bars.
	const TArray<USkeletalMeshComponent*> HairParts = {
		HairScalpMesh, HairBaseMesh, HairFrontMesh, HairSideMesh, HairBackMesh, HairExtraMesh};
	for (USkeletalMeshComponent* HairPart : HairParts)
	{
		HairPart->SetLeaderPoseComponent(nullptr);
	}
}

void AUECustomizationPreviewActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	InitializeCatalogs();
	Appearance.Normalize();
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
	SaveGame->DataVersion = 8;
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
	Appearance.Normalize();
	return true;
}

void AUECustomizationPreviewActor::CreateMaterialsForComponent(
	USkeletalMeshComponent* Component,
	TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMaterials)
{
	if (!Component || !Component->GetSkeletalMeshAsset())
	{
		return;
	}
	Component->EmptyOverrideMaterials();
	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		if (UMaterialInterface* Parent = Component->GetMaterial(Index))
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this);
			Component->SetMaterial(Index, Material);
			OutMaterials.Add(Material);
		}
	}
}

void AUECustomizationPreviewActor::CreateMaskedOverlayMaterials(
	USkeletalMeshComponent* Component,
	TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMaterials)
{
	if (!Component || !Component->GetSkeletalMeshAsset())
	{
		return;
	}
	Component->EmptyOverrideMaterials();
	UMaterialInterface* FaceLayerTemplate = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/Brow.Brow"));
	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* SourceMaterial = FaceLayerTemplate
			? FaceLayerTemplate
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
	OutfitMaterials.Empty();
	AccessoryMaterials.Empty();

	CreateMaterialsForComponent(BodyMesh, BodySkinMaterials);
	CreateOpaqueFaceMaterials(FaceSkinMesh, FaceSkinMaterials);
	CreateMaskedOverlayMaterials(EyeIrisMesh, EyeMaterials);
	CreateOpaqueFaceMaterials(
		EyeWhiteMesh,
		EyeWhiteMaterials,
		LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/VRoidCatalog/FaceGeometryDetermined/EyeWhiteOpaque.EyeWhiteOpaque")));
	CreateMaskedOverlayMaterials(EyeHighlightMesh, EyeHighlightMaterials);
	CreateMaskedOverlayMaterials(EyeExtraMesh, EyeExtraMaterials);
	CreateMaskedOverlayMaterials(BrowMesh, BrowMaterials);
	CreateMaskedOverlayMaterials(EyelashMesh, EyelashMaterials);
	CreateMaskedOverlayMaterials(EyelineMesh, EyelineMaterials);
	CreateMaskedOverlayMaterials(MouthMesh, MouthMaterials);
	CreateMaskedOverlayMaterials(LipOverlayMesh, LipMaterials);
	CreateMaskedOverlayMaterials(MouthLineOverlayMesh, MouthLineMaterials);
	CreateMaterialsForComponent(HairScalpMesh, HairScalpMaterials);
	HairMaterials.Append(HairScalpMaterials);
	CreateMaterialsForComponent(HairBaseMesh, HairBaseMaterials);
	HairMaterials.Append(HairBaseMaterials);
	CreateMaterialsForComponent(HairFrontMesh, HairMaterials);
	CreateMaterialsForComponent(HairSideMesh, HairMaterials);
	CreateMaterialsForComponent(HairBackMesh, HairMaterials);
	CreateMaterialsForComponent(HairExtraMesh, HairMaterials);
	CreateMaterialsForComponent(TopMesh, OutfitMaterials);
	CreateMaterialsForComponent(BottomMesh, OutfitMaterials);
	CreateMaterialsForComponent(OnepieceMesh, OutfitMaterials);
	CreateMaterialsForComponent(ShoesMesh, OutfitMaterials);
	CreateMaterialsForComponent(HeadAccessoryMesh, AccessoryMaterials);
	CreateMaterialsForComponent(FaceAccessoryMesh, AccessoryMaterials);
	CreateMaterialsForComponent(EarAccessoryMesh, AccessoryMaterials);
	CreateMaterialsForComponent(TailAccessoryMesh, AccessoryMaterials);
	CreateMaterialsForComponent(NeckAccessoryMesh, AccessoryMaterials);
	ApplySelectedTextures();
	ApplyNeutralMaterialLighting();
}

void AUECustomizationPreviewActor::ApplySelectedTextures()
{
	auto Apply = [this](
		EUECustomizationPart Part,
		int32 Index,
		USkeletalMeshComponent* Component,
		const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials)
	{
		UTexture2D* Texture = GetOptionTexture(Part, Index);
		UTexture2D* NormalTexture = Part == EUECustomizationPart::FaceSkin &&
			FaceSkinNormalTextureCatalog.IsValidIndex(Index)
			? FaceSkinNormalTextureCatalog[Index].Get()
			: nullptr;
		if (Component)
		{
			Component->SetVisibility(Texture != nullptr, true);
		}
		if (!Texture)
		{
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
			Material->SetScalarParameterValue(TEXT("OpacityMaskMapWeight"), 1.0f);
			Material->SetScalarParameterValue(TEXT("NormalMapWeight"), NormalTexture ? 1.0f : 0.0f);
			if (NormalTexture)
			{
				Material->SetTextureParameterValue(TEXT("NormalMap"), NormalTexture);
			}
		}
	};

	Apply(EUECustomizationPart::FaceSkin, Appearance.FaceStyle, FaceSkinMesh, FaceSkinMaterials);
	Apply(EUECustomizationPart::EyeWhite, Appearance.EyeWhiteStyle, EyeWhiteMesh, EyeWhiteMaterials);
	Apply(EUECustomizationPart::EyeIris, Appearance.EyeIrisStyle, EyeIrisMesh, EyeMaterials);
	Apply(EUECustomizationPart::EyeHighlight, Appearance.EyeHighlightStyle, EyeHighlightMesh, EyeHighlightMaterials);
	Apply(EUECustomizationPart::Brow, Appearance.BrowStyle, BrowMesh, BrowMaterials);
	Apply(EUECustomizationPart::Eyelash, Appearance.EyelashStyle, EyelashMesh, EyelashMaterials);
	Apply(EUECustomizationPart::Eyeline, Appearance.EyelineStyle, EyelineMesh, EyelineMaterials);
	Apply(EUECustomizationPart::Mouth, Appearance.MouthStyle, MouthMesh, MouthMaterials);
	Apply(EUECustomizationPart::Lip, Appearance.LipStyle, LipOverlayMesh, LipMaterials);
	Apply(EUECustomizationPart::MouthLine, Appearance.MouthLineStyle, MouthLineOverlayMesh, MouthLineMaterials);
}

void AUECustomizationPreviewActor::ApplyColors()
{
	auto SetMaterialScalar = [](
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
	};
	auto FlattenMaskedLayer = [&SetMaterialScalar](
		const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials)
	{
		SetMaterialScalar(Materials, TEXT("DiffuseColorMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("EmissiveColorMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("SpecularColorMapWeight"), 0.0f);
		SetMaterialScalar(Materials, TEXT("Shininess"), 0.0f);
	};
	const FLinearColor ReferenceSkinColor = CatalogAsset
		? CatalogAsset->ReferenceFaceSkinColor
		: FLinearColor::White;
	const FLinearColor SkinTint = MakeFaceSkinTint(Appearance.SkinColor, ReferenceSkinColor);
	const FLinearColor BodySkinGain = CatalogAsset
		? CatalogAsset->BodySkinColorGain
		: FLinearColor(1.15f, 1.50f, 1.50f, 1.0f);
	const FLinearColor BodySkinColor(
		FMath::Clamp(Appearance.SkinColor.R * BodySkinGain.R, 0.0f, 1.0f),
		FMath::Clamp(Appearance.SkinColor.G * BodySkinGain.G, 0.0f, 1.0f),
		FMath::Clamp(Appearance.SkinColor.B * BodySkinGain.B, 0.0f, 1.0f),
		Appearance.SkinColor.A);
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
	SetMaterialColor(BodySkinMaterials, DiffuseColorParameter, BodySkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("EmissiveColor"), BodySkinColor);
	SetMaterialColor(BodySkinMaterials, TEXT("TintColor"), BodySkinColor);
	SetMaterialScalar(BodySkinMaterials, TEXT("DiffuseColorMapWeight"), 0.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("EmissiveColorMapWeight"), 0.0f);
	SetMaterialScalar(BodySkinMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialColor(FaceSkinMaterials, DiffuseColorParameter, SkinTint);
	SetMaterialColor(FaceSkinMaterials, TEXT("EmissiveColor"), SkinTint);
	SetMaterialColor(HairMaterials, DiffuseColorParameter, Appearance.HairColor);
	SetMaterialScalar(HairScalpMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(HairScalpMaterials, TEXT("OpacityMaskMapWeight"), 0.0f);
	SetMaterialColor(EyeMaterials, DiffuseColorParameter, Appearance.EyeColor);
	SetMaterialColor(EyeMaterials, TEXT("EmissiveColor"), Appearance.EyeColor);
	SetMaterialColor(EyeMaterials, TEXT("TintColor"), Appearance.EyeColor);
	SetMaterialScalar(EyeMaterials, TEXT("DiffuseColorMapWeight"), 0.68f);
	SetMaterialScalar(EyeMaterials, TEXT("EmissiveColorMapWeight"), 0.32f);
	SetMaterialColor(EyeWhiteMaterials, DiffuseColorParameter, FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("EmissiveColor"), FLinearColor::White);
	SetMaterialColor(EyeWhiteMaterials, TEXT("TintColor"), FLinearColor::White);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("DiffuseColorMapWeight"), 0.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("OpacityMask"), 1.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("OpacityMaskMapWeight"), 0.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("EmissiveColorMapWeight"), 0.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(EyeWhiteMaterials, TEXT("Shininess"), 0.0f);
	SetMaterialColor(EyeHighlightMaterials, DiffuseColorParameter, FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("EmissiveColor"), FLinearColor::White);
	SetMaterialColor(EyeHighlightMaterials, TEXT("TintColor"), FLinearColor::White);
	SetMaterialColor(EyeExtraMaterials, DiffuseColorParameter, Appearance.EyeColor);
	SetMaterialColor(EyeExtraMaterials, TEXT("EmissiveColor"), Appearance.EyeColor);
	SetMaterialColor(EyeExtraMaterials, TEXT("TintColor"), Appearance.EyeColor);
	SetMaterialColor(BrowMaterials, DiffuseColorParameter, BrowColor);
	SetMaterialColor(BrowMaterials, TEXT("EmissiveColor"), BrowColor);
	SetMaterialColor(BrowMaterials, TEXT("TintColor"), BrowColor);
	SetMaterialColor(BrowMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	FlattenMaskedLayer(BrowMaterials);
	SetMaterialColor(EyelashMaterials, DiffuseColorParameter, LashColor);
	SetMaterialColor(EyelashMaterials, TEXT("EmissiveColor"), LashColor);
	SetMaterialColor(EyelashMaterials, TEXT("TintColor"), LashColor);
	SetMaterialColor(EyelashMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	FlattenMaskedLayer(EyelashMaterials);
	SetMaterialColor(EyelineMaterials, DiffuseColorParameter, LashColor);
	SetMaterialColor(EyelineMaterials, TEXT("EmissiveColor"), LashColor);
	SetMaterialColor(EyelineMaterials, TEXT("TintColor"), LashColor);
	SetMaterialColor(EyelineMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	FlattenMaskedLayer(EyelineMaterials);
	SetMaterialColor(MouthMaterials, DiffuseColorParameter, FLinearColor::White);
	SetMaterialColor(MouthMaterials, TEXT("EmissiveColor"), FLinearColor::White);
	SetMaterialColor(MouthMaterials, TEXT("TintColor"), FLinearColor::White);
	SetMaterialColor(LipMaterials, DiffuseColorParameter, Appearance.LipColor);
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
	SetMaterialColor(MouthLineMaterials, TEXT("EmissiveColor"), MouthLineColor);
	SetMaterialColor(MouthLineMaterials, TEXT("TintColor"), MouthLineColor);
	SetMaterialColor(MouthLineMaterials, TEXT("SpecularColor"), FLinearColor::Black);
	SetMaterialScalar(MouthLineMaterials, TEXT("DiffuseColorMapWeight"), 0.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("EmissiveColorMapWeight"), 0.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("OpacityMaskMapWeight"), 1.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("SpecularColorMapWeight"), 0.0f);
	SetMaterialScalar(MouthLineMaterials, TEXT("Shininess"), 0.0f);
	SetMaterialColor(OutfitMaterials, DiffuseColorParameter, Appearance.OutfitColor);
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

	const float HeadScale = FMath::Lerp(0.94f, 1.06f, Appearance.HeadSize);
	const bool bFemale = Appearance.Gender == EUECharacterGender::Female;
	const float HeadPivotY = CatalogAsset
		? (bFemale ? CatalogAsset->FemaleHeadPivotY : CatalogAsset->MaleHeadPivotY)
		: 0.0f;
	const float HeadPivotZ = CatalogAsset
		? (bFemale ? CatalogAsset->FemaleHeadPivotZ : CatalogAsset->MaleHeadPivotZ)
		: 0.0f;
	const float HeadVerticalOffset = CatalogAsset
		? (bFemale ? CatalogAsset->FemaleHeadVerticalOffset : CatalogAsset->MaleHeadVerticalOffset)
		: 0.0f;
	const FVector HeadOffset(
		0.0f,
		HeadPivotY * (1.0f - HeadScale),
		HeadPivotZ * (1.0f - HeadScale) + HeadVerticalOffset);
	const TArray<USkeletalMeshComponent*> FaceParts = {
		FaceSkinMesh, EyeWhiteMesh, EyeIrisMesh, EyeHighlightMesh, EyeExtraMesh, BrowMesh, EyelashMesh,
		EyelineMesh, MouthMesh, LipOverlayMesh, MouthLineOverlayMesh};
	for (USkeletalMeshComponent* Part : FaceParts)
	{
		Part->SetRelativeLocation(HeadOffset);
		Part->SetRelativeScale3D(FVector(HeadScale));
	}
	const float HairRadialScale = HeadScale * (CatalogAsset ? CatalogAsset->HairRadialScale : 1.0f);
	const FVector HairOffset(
		0.0f,
		HeadPivotY * (1.0f - HairRadialScale),
		HeadPivotZ * (1.0f - HeadScale) + HeadVerticalOffset);
	const TArray<USkeletalMeshComponent*> HairParts = {
		HairBaseMesh, HairFrontMesh, HairSideMesh, HairBackMesh, HairExtraMesh};
	for (USkeletalMeshComponent* Part : HairParts)
	{
		Part->SetRelativeLocation(HairOffset);
		Part->SetRelativeScale3D(FVector(HairRadialScale, HairRadialScale, HeadScale));
	}
	HeadAccessoryMesh->SetRelativeLocation(HeadOffset + FVector(
		0.0f,
		0.0f,
		CatalogAsset ? CatalogAsset->HeadAccessoryVerticalOffset : 9.0f));
	HeadAccessoryMesh->SetRelativeScale3D(FVector(HeadScale));
	EarAccessoryMesh->SetRelativeLocation(HeadOffset);
	EarAccessoryMesh->SetRelativeScale3D(FVector(HeadScale));
	FaceAccessoryMesh->SetRelativeLocation(HeadOffset + FVector(
		0.0f,
		CatalogAsset ? CatalogAsset->FaceAccessoryForwardOffset : 6.5f,
		CatalogAsset ? CatalogAsset->FaceAccessoryVerticalOffset : 17.5f));
	FaceAccessoryMesh->SetRelativeScale3D(FVector(HeadScale));
	const float ScalpRadialScale = HairRadialScale *
		(CatalogAsset ? CatalogAsset->HairScalpInsetScale : 0.985f);
	HairScalpMesh->SetRelativeLocation(FVector(
		0.0f,
		HeadPivotY * (1.0f - ScalpRadialScale),
		HeadPivotZ * (1.0f - HeadScale) + HeadVerticalOffset));
	HairScalpMesh->SetRelativeScale3D(FVector(ScalpRadialScale, ScalpRadialScale, HeadScale));
	EyeWhiteMesh->AddRelativeLocation(FVector(
		0.0f,
		CatalogAsset ? CatalogAsset->ScleraDepthOffsetY : 0.0f,
		0.0f));
	LipOverlayMesh->SetRelativeScale3D(FVector(HeadScale * 1.0008f));
	MouthLineOverlayMesh->SetRelativeScale3D(FVector(HeadScale * 1.0012f));
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
