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
	HeadEquipmentMesh = CreateSkeletalPart(this, CharacterRoot, TEXT("HeadEquipment"));

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
	const float ClampedValue = 1.0f;
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
	if (Appearance.HeadEquipmentIndex >= 0)
	{
		Appearance.HeadEquipmentIndex = ClampIndex(Appearance.HeadEquipmentIndex, GetOptionCount(EUEPalworldCustomizationCategory::HeadEquipment));
	}

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
	const FUEPalworldCustomizationOption& HeadEquipment = Appearance.HeadEquipmentIndex >= 0
		? GetOption(EUEPalworldCustomizationCategory::HeadEquipment, Appearance.HeadEquipmentIndex)
		: PreviewEmptyOption;

	BodyEquipmentMesh->SetSkeletalMesh(BodyEquipment.LoadMesh(Appearance.Gender));
	HeadMesh->SetSkeletalMesh(Head.LoadMesh(Appearance.Gender));
	HairMesh->SetSkeletalMesh(Hair.LoadMesh(Appearance.Gender));
	HeadEquipmentMesh->SetSkeletalMesh(HeadEquipment.LoadMesh(Appearance.Gender));

	RefreshFollowerPose();
	AttachHeadEquipment(HeadEquipment);
	ApplyEyeMaterial(Eyes);
	ApplyMaterialColors();
	ApplyScale();
	FitHeadEquipmentToHead(HeadEquipment);
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
	if (FParse::Value(FCommandLine::Get(), TEXT("PalworldQAAccessory="), Value))
	{
		Appearance.HeadEquipmentIndex = FCString::Atoi(*Value);
	}
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
	LogPart(TEXT("HeadEquipment"), HeadEquipmentMesh);

	UE_LOG(LogTemp, Display,
		TEXT("Palworld QA selection gender=%s body=%d head=%d hair=%d eyes=%d outfit=%d accessory=%d"),
		Appearance.Gender == EUEPalworldGender::TypeA ? TEXT("TypeA") : TEXT("TypeB"),
		Appearance.BodyIndex,
		Appearance.HeadIndex,
		Appearance.HairIndex,
		Appearance.EyeIndex,
		Appearance.BodyEquipmentIndex,
		Appearance.HeadEquipmentIndex);

	const TCHAR* GenderName = Appearance.Gender == EUEPalworldGender::TypeA ? TEXT("TypeA") : TEXT("TypeB");
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		FString::Printf(
			TEXT("Screenshots/Customization/Palworld_%s_H%02d_Hair%02d_Eye%02d_Outfit%02d_Acc%02d_Full.png"),
			GenderName,
			Appearance.HeadIndex,
			Appearance.HairIndex,
			Appearance.EyeIndex,
			Appearance.BodyEquipmentIndex,
			Appearance.HeadEquipmentIndex);
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
			TEXT("Screenshots/Customization/Palworld_%s_H%02d_Hair%02d_Eye%02d_Outfit%02d_Acc%02d_Head.png"),
			GenderName,
			Appearance.HeadIndex,
			Appearance.HairIndex,
			Appearance.EyeIndex,
			Appearance.BodyEquipmentIndex,
			Appearance.HeadEquipmentIndex);
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

void AUEPalworldCustomizationPreviewActor::RefreshFollowerPose()
{
	USkeletalMesh* LeaderMesh = BodyEquipmentMesh ? BodyEquipmentMesh->GetSkeletalMeshAsset() : nullptr;
	const USkeleton* LeaderSkeleton = LeaderMesh ? LeaderMesh->GetSkeleton() : nullptr;

	for (USkeletalMeshComponent* Follower : {HeadMesh.Get(), HairMesh.Get(), HeadEquipmentMesh.Get()})
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

void AUEPalworldCustomizationPreviewActor::AttachHeadEquipment(const FUEPalworldCustomizationOption& Option)
{
	bHeadEquipmentAttachedToSocket = false;
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
	USceneComponent* AttachParent = CharacterRoot;
	FName AttachSocket = NAME_None;
	const FName TableSocket = Option.GetAttachSocket(Appearance.Gender);

	if (!TableSocket.IsNone())
	{
		const TArray<USkeletalMeshComponent*> SocketOwners = Option.bIsHairAttachAccessory
			? TArray<USkeletalMeshComponent*>{HairMesh.Get(), HeadMesh.Get(), BodyEquipmentMesh.Get()}
			: TArray<USkeletalMeshComponent*>{HeadMesh.Get(), BodyEquipmentMesh.Get(), HairMesh.Get()};

		for (USkeletalMeshComponent* Candidate : SocketOwners)
		{
			if (Candidate && Candidate->DoesSocketExist(TableSocket))
			{
				AttachParent = Candidate;
				AttachSocket = TableSocket;
				bHeadEquipmentAttachedToSocket = true;
				break;
			}
		}
	}

	HeadEquipmentMesh->AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
	HeadEquipmentMesh->SetRelativeTransform(FTransform::Identity);
}

void AUEPalworldCustomizationPreviewActor::FitHeadEquipmentToHead(const FUEPalworldCustomizationOption& Option)
{
	if (bHeadEquipmentAttachedToSocket || !HeadEquipmentMesh || !HeadEquipmentMesh->GetSkeletalMeshAsset() || !HeadMesh)
	{
		return;
	}

	HeadEquipmentMesh->UpdateBounds();
	HeadMesh->UpdateBounds();
	if (HairMesh)
	{
		HairMesh->UpdateBounds();
	}

	const FBox HeadBox = HeadMesh->Bounds.GetBox();
	const FBox HairBox = HairMesh && HairMesh->GetSkeletalMeshAsset()
		? HairMesh->Bounds.GetBox()
		: HeadBox;
	const FBox TargetBox = HeadBox + HairBox;
	const FBox EquipmentBox = HeadEquipmentMesh->Bounds.GetBox();

	if (!TargetBox.IsValid || !EquipmentBox.IsValid)
	{
		return;
	}

	const float HeadHeight = FMath::Max(1.0f, HeadBox.GetSize().Z);
	const FString SocketName = Option.GetAttachSocket(Appearance.Gender).ToString().ToLower();
	FVector TargetPoint = HeadBox.GetCenter();
	FVector EquipmentAnchor = EquipmentBox.GetCenter();

	// CUE4Parse/FBX import keeps the Palworld table socket names, but not every socket object
	// survives on the imported skeleton. When a socket is absent, use the table name as a
	// placement hint instead of leaving the gear at the component origin.
	if (!SocketName.IsEmpty() && SocketName != TEXT("none"))
	{
		if (SocketName.Contains(TEXT("ear")))
		{
			const bool bLeftSide = SocketName.EndsWith(TEXT("_l")) || SocketName.EndsWith(TEXT("l"));
			TargetPoint = FVector(
				bLeftSide ? HeadBox.Min.X : HeadBox.Max.X,
				HeadBox.GetCenter().Y,
				HeadBox.GetCenter().Z + HeadHeight * 0.12f);
			EquipmentAnchor = FVector(
				bLeftSide ? EquipmentBox.Max.X : EquipmentBox.Min.X,
				EquipmentBox.GetCenter().Y,
				EquipmentBox.GetCenter().Z);
		}
		else if (SocketName.Contains(TEXT("top")) || SocketName.Contains(TEXT("root")))
		{
			TargetPoint = FVector(
				TargetBox.GetCenter().X,
				TargetBox.GetCenter().Y,
				TargetBox.Max.Z - HeadHeight * 0.04f);
			EquipmentAnchor = FVector(
				EquipmentBox.GetCenter().X,
				EquipmentBox.GetCenter().Y,
				EquipmentBox.Min.Z);
		}
		else if (SocketName.Contains(TEXT("front")))
		{
			TargetPoint = FVector(
				HeadBox.GetCenter().X,
				FMath::Max(HeadBox.Max.Y, TargetBox.Max.Y) + 1.0f,
				HeadBox.Max.Z - HeadHeight * 0.18f);
			EquipmentAnchor = FVector(
				EquipmentBox.GetCenter().X,
				EquipmentBox.Min.Y,
				EquipmentBox.Min.Z + EquipmentBox.GetSize().Z * 0.25f);
		}
	}

	const FVector WorldDelta = TargetPoint - EquipmentAnchor;
	const FVector LocalDelta = CharacterRoot->GetComponentTransform().InverseTransformVectorNoScale(WorldDelta);
	HeadEquipmentMesh->AddLocalOffset(LocalDelta);
}

void AUEPalworldCustomizationPreviewActor::ApplyMaterialColors()
{
	// Palworld's original MI assets already carry the authored outfit/accessory colors.
	// Runtime tint is kept as a Blueprint API for later palette work, but white means "use source material".
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
	if (!Appearance.BodyEquipmentColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyColorToSlots(BodyEquipmentMesh, Appearance.BodyEquipmentColor, {TEXT("Outfit"), TEXT("Equip")});
	}
	if (!Appearance.HeadEquipmentColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyColorToSlots(HeadEquipmentMesh, Appearance.HeadEquipmentColor, {TEXT("Head"), TEXT("Hat"), TEXT("Helmet"), TEXT("Accessory")});
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

void AUEPalworldCustomizationPreviewActor::ApplyScale()
{
	// Do not scale modular parts independently. These extracted Palworld meshes only stay
	// attached correctly when body, head, hair, and equipment share the same reference scale.
	Appearance.HeightScale = 1.0f;
	Appearance.HeadScale = 1.0f;
	Appearance.BodyWidthScale = 1.0f;
	CharacterRoot->SetRelativeScale3D(FVector::OneVector);

	for (USkeletalMeshComponent* Component : {BodyEquipmentMesh.Get(), HeadMesh.Get(), HairMesh.Get(), HeadEquipmentMesh.Get()})
	{
		if (Component)
		{
			Component->SetRelativeScale3D(FVector::OneVector);
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
