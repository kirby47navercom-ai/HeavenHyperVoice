#include "UEPalworldCustomizationPreviewActor.h"

#include "../Data/UEPalworldCustomizationTypes.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FUEPalworldCustomizationOption EmptyOption;

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
	HeadEquipmentMesh = CreateSkeletalPart(this, CharacterRoot, TEXT("HeadEquipment"));

	PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
	PreviewCamera->SetupAttachment(SceneRoot);
	PreviewCamera->SetRelativeLocation(FVector(-230.0f, 0.0f, 125.0f));
	PreviewCamera->SetRelativeRotation(FRotator(-2.0f, 0.0f, 0.0f));

	KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetRelativeLocation(FVector(-140.0f, -110.0f, 230.0f));
	KeyLight->SetRelativeRotation(FRotator(-42.0f, 35.0f, 0.0f));
	KeyLight->SetIntensity(5000.0f);
	KeyLight->SetAttenuationRadius(600.0f);
}

void AUEPalworldCustomizationPreviewActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshMeshes();
}

void AUEPalworldCustomizationPreviewActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshMeshes();
}

void AUEPalworldCustomizationPreviewActor::ApplyAppearance(const FUEPalworldAppearance& NewAppearance)
{
	Appearance = NewAppearance;
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
	RefreshMeshes();
}

void AUEPalworldCustomizationPreviewActor::SetColor(EUEPalworldColorChannel Channel, const FLinearColor& Color)
{
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
	case EUEPalworldColorChannel::BodyEquipment:
		Appearance.BodyEquipmentColor = ClampedColor;
		break;
	case EUEPalworldColorChannel::HeadEquipment:
		Appearance.HeadEquipmentColor = ClampedColor;
		break;
	default:
		break;
	}
	RefreshMeshes();
}

void AUEPalworldCustomizationPreviewActor::SetScaleValue(EUEPalworldScaleChannel Channel, float Value)
{
	const float ClampedValue = FMath::Clamp(Value, 0.75f, 1.25f);
	switch (Channel)
	{
	case EUEPalworldScaleChannel::Height:
		Appearance.HeightScale = ClampedValue;
		break;
	case EUEPalworldScaleChannel::HeadSize:
		Appearance.HeadScale = ClampedValue;
		break;
	case EUEPalworldScaleChannel::BodyWidth:
		Appearance.BodyWidthScale = ClampedValue;
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

	Appearance.BodyIndex = ClampIndex(Appearance.BodyIndex, GetOptionCount(EUEPalworldCustomizationCategory::Body));
	Appearance.HeadIndex = ClampIndex(Appearance.HeadIndex, GetOptionCount(EUEPalworldCustomizationCategory::Head));
	Appearance.HairIndex = ClampIndex(Appearance.HairIndex, GetOptionCount(EUEPalworldCustomizationCategory::Hair));
	Appearance.EyeIndex = ClampIndex(Appearance.EyeIndex, GetOptionCount(EUEPalworldCustomizationCategory::Eyes));
	Appearance.BodyEquipmentIndex = ClampIndex(Appearance.BodyEquipmentIndex, GetOptionCount(EUEPalworldCustomizationCategory::BodyEquipment));
	Appearance.HeadEquipmentIndex = ClampIndex(Appearance.HeadEquipmentIndex, GetOptionCount(EUEPalworldCustomizationCategory::HeadEquipment));

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
	const FUEPalworldCustomizationOption& HeadEquipment = GetOption(
		EUEPalworldCustomizationCategory::HeadEquipment,
		Appearance.HeadEquipmentIndex);

	BodyEquipmentMesh->SetSkeletalMesh(BodyEquipment.LoadMesh(Appearance.Gender));
	HeadMesh->SetSkeletalMesh(Head.LoadMesh(Appearance.Gender));
	HairMesh->SetSkeletalMesh(Hair.LoadMesh(Appearance.Gender));
	HeadEquipmentMesh->SetSkeletalMesh(HeadEquipment.LoadMesh(Appearance.Gender));

	RefreshFollowerPose();
	AttachHeadEquipment(HeadEquipment);
	ApplyEyeMaterial(Eyes);
	ApplyMaterialColors();
	ApplyScale();
}

void AUEPalworldCustomizationPreviewActor::RefreshFollowerPose()
{
	if (!BodyEquipmentMesh)
	{
		return;
	}

	for (USkeletalMeshComponent* Follower : {HeadMesh.Get(), HairMesh.Get(), HeadEquipmentMesh.Get()})
	{
		if (!Follower)
		{
			continue;
		}
		Follower->SetLeaderPoseComponent(BodyEquipmentMesh, true, false);
		Follower->SetRelativeTransform(FTransform::Identity);
		Follower->SetVisibility(Follower->GetSkeletalMeshAsset() != nullptr, true);
	}
}

void AUEPalworldCustomizationPreviewActor::AttachHeadEquipment(const FUEPalworldCustomizationOption& Option)
{
	if (!HeadEquipmentMesh)
	{
		return;
	}

	if (!HeadEquipmentMesh->GetSkeletalMeshAsset())
	{
		HeadEquipmentMesh->SetVisibility(false, true);
		return;
	}

	HeadEquipmentMesh->SetVisibility(true, true);
	HeadEquipmentMesh->AttachToComponent(CharacterRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	HeadEquipmentMesh->SetRelativeTransform(FTransform::Identity);
}

void AUEPalworldCustomizationPreviewActor::ApplyMaterialColors()
{
	ApplyColorToSlots(HeadMesh, Appearance.SkinColor, {TEXT("Head"), TEXT("Body")});
	ApplyColorToSlots(BodyEquipmentMesh, Appearance.SkinColor, {TEXT("Body")});
	ApplyColorToSlots(HairMesh, Appearance.HairColor, {TEXT("Hair")});
	ApplyColorToSlots(HeadMesh, Appearance.EyeColor, {TEXT("Eye")});
	ApplyColorToSlots(BodyEquipmentMesh, Appearance.BodyEquipmentColor, {});
	ApplyColorToSlots(HeadEquipmentMesh, Appearance.HeadEquipmentColor, {});
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

void AUEPalworldCustomizationPreviewActor::ApplyScale()
{
	const FVector BodyScale(
		FMath::Max(0.01f, Appearance.BodyWidthScale),
		FMath::Max(0.01f, Appearance.BodyWidthScale),
		FMath::Max(0.01f, Appearance.HeightScale));
	CharacterRoot->SetRelativeScale3D(BodyScale);

	const FVector HeadScale(FMath::Max(0.01f, Appearance.HeadScale));
	for (USkeletalMeshComponent* Component : {HeadMesh.Get(), HairMesh.Get(), HeadEquipmentMesh.Get()})
	{
		if (Component)
		{
			Component->SetRelativeScale3D(HeadScale);
		}
	}
}

const FUEPalworldCustomizationOption& AUEPalworldCustomizationPreviewActor::GetOption(
	EUEPalworldCustomizationCategory Category,
	int32 Index) const
{
	return Catalog ? Catalog->GetOption(Category, Index) : EmptyOption;
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
	case EUEPalworldCustomizationCategory::HeadEquipment:
		return Appearance.HeadEquipmentIndex;
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
	case EUEPalworldCustomizationCategory::HeadEquipment:
		return Appearance.HeadEquipmentIndex;
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
