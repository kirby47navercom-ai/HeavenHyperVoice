#include "UECustomizationPreviewActor.h"

#include "../Data/UECharacterCustomizationSaveGame.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

const FString AUECustomizationPreviewActor::SaveSlotName(TEXT("UECharacterAppearance"));

AUECustomizationPreviewActor::AUECustomizationPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CharacterRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CharacterRoot"));
	CharacterRoot->SetupAttachment(SceneRoot);

	PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
	PreviewCamera->SetupAttachment(SceneRoot);
	PreviewCamera->SetRelativeLocation(FVector(780.0f, 0.0f, 122.0f));
	PreviewCamera->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	PreviewCamera->FieldOfView = 38.0f;
	PreviewCamera->bAutoActivate = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	SphereMesh = SphereFinder.Object;
	CubeMesh = CubeFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	ConeMesh = ConeFinder.Object;
	BaseMaterial = MaterialFinder.Object;

	Head = CreatePart(TEXT("Head"));
	Hair = CreatePart(TEXT("Hair"));
	Torso = CreatePart(TEXT("Torso"));
	Pelvis = CreatePart(TEXT("Pelvis"));
	LeftArm = CreatePart(TEXT("LeftArm"));
	RightArm = CreatePart(TEXT("RightArm"));
	LeftLeg = CreatePart(TEXT("LeftLeg"));
	RightLeg = CreatePart(TEXT("RightLeg"));
	LeftBoot = CreatePart(TEXT("LeftBoot"));
	RightBoot = CreatePart(TEXT("RightBoot"));
	Accessory = CreatePart(TEXT("Accessory"));

	Head->SetStaticMesh(SphereMesh);
	Hair->SetStaticMesh(SphereMesh);
	Torso->SetStaticMesh(CylinderMesh);
	Pelvis->SetStaticMesh(CubeMesh);
	LeftArm->SetStaticMesh(CylinderMesh);
	RightArm->SetStaticMesh(CylinderMesh);
	LeftLeg->SetStaticMesh(CylinderMesh);
	RightLeg->SetStaticMesh(CylinderMesh);
	LeftBoot->SetStaticMesh(CubeMesh);
	RightBoot->SetStaticMesh(CubeMesh);
	Accessory->SetStaticMesh(ConeMesh);
}

UStaticMeshComponent* AUECustomizationPreviewActor::CreatePart(const FName& PartName)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(PartName);
	Part->SetupAttachment(CharacterRoot);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCastShadow(true);
	return Part;
}

void AUECustomizationPreviewActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Appearance.Normalize();
	ApplyTransforms();
}

void AUECustomizationPreviewActor::BeginPlay()
{
	Super::BeginPlay();
	CreateDynamicMaterials();
	LoadAppearance();
	ApplyAppearance(Appearance);
}

void AUECustomizationPreviewActor::ApplyAppearance(const FUECharacterCustomizationData& NewAppearance)
{
	Appearance = NewAppearance;
	Appearance.Normalize();
	ApplyTransforms();
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
	RandomAppearance.BodyPreset = static_cast<EUEBodyPreset>(FMath::RandRange(0, 2));
	RandomAppearance.Height = FMath::FRand();
	RandomAppearance.HeadSize = FMath::FRand();
	RandomAppearance.ShoulderWidth = FMath::FRand();
	RandomAppearance.HairStyle = FMath::RandRange(0, 3);
	RandomAppearance.AccessoryStyle = FMath::RandRange(0, 2);

	const TArray<FLinearColor> SkinColors = {
		FLinearColor(0.92f, 0.72f, 0.58f), FLinearColor(0.72f, 0.48f, 0.34f), FLinearColor(0.30f, 0.16f, 0.10f)};
	const TArray<FLinearColor> HairColors = {
		FLinearColor(0.025f, 0.04f, 0.07f), FLinearColor(0.02f, 0.42f, 0.78f), FLinearColor(0.85f, 0.12f, 0.18f)};
	const TArray<FLinearColor> OutfitColors = {
		FLinearColor(0.02f, 0.55f, 0.95f), FLinearColor(0.95f, 0.72f, 0.04f), FLinearColor(0.88f, 0.08f, 0.12f)};

	RandomAppearance.SkinColor = SkinColors[FMath::RandRange(0, SkinColors.Num() - 1)];
	RandomAppearance.HairColor = HairColors[FMath::RandRange(0, HairColors.Num() - 1)];
	RandomAppearance.OutfitColor = OutfitColors[FMath::RandRange(0, OutfitColors.Num() - 1)];
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
	Appearance.Normalize();
	return true;
}

void AUECustomizationPreviewActor::CreateDynamicMaterials()
{
	if (!BaseMaterial)
	{
		return;
	}

	SkinMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this, TEXT("MID_Skin"));
	HairMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this, TEXT("MID_Hair"));
	OutfitMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this, TEXT("MID_Outfit"));

	Head->SetMaterial(0, SkinMaterial);
	LeftArm->SetMaterial(0, SkinMaterial);
	RightArm->SetMaterial(0, SkinMaterial);
	Hair->SetMaterial(0, HairMaterial);

	const TArray<UStaticMeshComponent*> OutfitParts = {
		Torso, Pelvis, LeftLeg, RightLeg, LeftBoot, RightBoot, Accessory};
	for (UStaticMeshComponent* Part : OutfitParts)
	{
		Part->SetMaterial(0, OutfitMaterial);
	}
}

void AUECustomizationPreviewActor::ApplyColors()
{
	SetMaterialColor(SkinMaterial, Appearance.SkinColor);
	SetMaterialColor(HairMaterial, Appearance.HairColor);
	SetMaterialColor(OutfitMaterial, Appearance.OutfitColor);
}

void AUECustomizationPreviewActor::ApplyTransforms()
{
	const float HeightScale = FMath::Lerp(0.88f, 1.12f, Appearance.Height);
	const float HeadScale = FMath::Lerp(0.25f, 0.38f, Appearance.HeadSize);
	const float ShoulderOffset = FMath::Lerp(43.0f, 61.0f, Appearance.ShoulderWidth);

	float BodyWidth = 1.0f;
	switch (Appearance.BodyPreset)
	{
	case EUEBodyPreset::Slim:
		BodyWidth = 0.82f;
		break;
	case EUEBodyPreset::Heavy:
		BodyWidth = 1.20f;
		break;
	default:
		BodyWidth = 1.0f;
		break;
	}

	CharacterRoot->SetRelativeScale3D(FVector(1.0f, 1.0f, HeightScale));
	Head->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
	Head->SetRelativeScale3D(FVector(HeadScale));
	Hair->SetRelativeLocation(FVector(0.0f, 0.0f, 235.0f));

	switch (Appearance.HairStyle)
	{
	case 1:
		Hair->SetStaticMesh(CubeMesh);
		Hair->SetRelativeScale3D(FVector(0.31f, 0.34f, 0.16f));
		break;
	case 2:
		Hair->SetStaticMesh(ConeMesh);
		Hair->SetRelativeScale3D(FVector(0.32f, 0.32f, 0.48f));
		break;
	case 3:
		Hair->SetStaticMesh(CylinderMesh);
		Hair->SetRelativeScale3D(FVector(0.34f, 0.34f, 0.18f));
		break;
	default:
		Hair->SetStaticMesh(SphereMesh);
		Hair->SetRelativeScale3D(FVector(0.34f, 0.34f, 0.20f));
		break;
	}

	Torso->SetRelativeLocation(FVector(0.0f, 0.0f, 152.0f));
	Torso->SetRelativeScale3D(FVector(0.38f * BodyWidth, 0.48f * BodyWidth, 0.54f));
	Pelvis->SetRelativeLocation(FVector(0.0f, 0.0f, 102.0f));
	Pelvis->SetRelativeScale3D(FVector(0.34f * BodyWidth, 0.44f * BodyWidth, 0.23f));

	LeftArm->SetRelativeLocation(FVector(0.0f, -ShoulderOffset, 150.0f));
	RightArm->SetRelativeLocation(FVector(0.0f, ShoulderOffset, 150.0f));
	LeftArm->SetRelativeScale3D(FVector(0.13f, 0.13f, 0.58f));
	RightArm->SetRelativeScale3D(FVector(0.13f, 0.13f, 0.58f));

	LeftLeg->SetRelativeLocation(FVector(0.0f, -23.0f * BodyWidth, 47.0f));
	RightLeg->SetRelativeLocation(FVector(0.0f, 23.0f * BodyWidth, 47.0f));
	LeftLeg->SetRelativeScale3D(FVector(0.16f * BodyWidth, 0.16f * BodyWidth, 0.55f));
	RightLeg->SetRelativeScale3D(FVector(0.16f * BodyWidth, 0.16f * BodyWidth, 0.55f));

	LeftBoot->SetRelativeLocation(FVector(12.0f, -23.0f * BodyWidth, -4.0f));
	RightBoot->SetRelativeLocation(FVector(12.0f, 23.0f * BodyWidth, -4.0f));
	LeftBoot->SetRelativeScale3D(FVector(0.36f, 0.18f * BodyWidth, 0.14f));
	RightBoot->SetRelativeScale3D(FVector(0.36f, 0.18f * BodyWidth, 0.14f));

	Accessory->SetVisibility(Appearance.AccessoryStyle != 0);
	Accessory->SetRelativeLocation(Appearance.AccessoryStyle == 1
		? FVector(-4.0f, 0.0f, 190.0f)
		: FVector(-16.0f, 0.0f, 170.0f));
	Accessory->SetRelativeRotation(Appearance.AccessoryStyle == 1
		? FRotator(180.0f, 0.0f, 0.0f)
		: FRotator(0.0f, 0.0f, 0.0f));
	Accessory->SetRelativeScale3D(Appearance.AccessoryStyle == 1
		? FVector(0.42f, 0.42f, 0.16f)
		: FVector(0.20f, 0.20f, 0.42f));
}

void AUECustomizationPreviewActor::SetMaterialColor(UMaterialInstanceDynamic* Material, const FLinearColor& Color) const
{
	if (!Material)
	{
		return;
	}

	// BasicShapeMaterial uses Color; BaseColor keeps this compatible with future project materials.
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
}
