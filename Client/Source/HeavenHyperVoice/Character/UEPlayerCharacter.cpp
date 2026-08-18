#include "UEPlayerCharacter.h"

#include "../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"
#include "../Player/Server/UEPlayerMovementSyncComponent.h"
#include "../Data/UEPlayerAnimationDataAsset.h"
#include "../Pokemon/UEPokemonCharacter.h"
#include "../Pokemon/UEPokemonSpeciesData.h"
#include "../Pokemon/Server/UEPokemonServerComponent.h"
#include "../Pokemon/Server/UEPokemonServerSubsystem.h"
#include "../Pokemon/UEPokemonWorldSubsystem.h"
#include "../System/UEGameInstance.h"
#include "../UEGameplayTags.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* HHVMorphSafeMaterialFolder = TEXT("/Game/CharacterCustomization/HHV/Generated/MorphSafeMaterials");
	const TCHAR* HHVEyeCompositeFolder = TEXT("/Game/CharacterCustomization/HHV/Generated/EyeComposite");
	constexpr int32 HHVMaxVisibleOutfits = 14;
	constexpr int32 HHVFirstVisibleOutfitIndex = 1;

	void ApplyPlayerHHVSignedMorphTarget(USkeletalMeshComponent* Component, const FName MinTarget, const FName MaxTarget, float Value)
	{
		if (!Component)
		{
			return;
		}

		const float ClampedValue = FMath::Clamp(Value, -1.0f, 1.0f);
		Component->SetMorphTarget(MinTarget, ClampedValue < 0.0f ? -ClampedValue : 0.0f);
		Component->SetMorphTarget(MaxTarget, ClampedValue > 0.0f ? ClampedValue : 0.0f);
	}

	int32 ClampOptionIndex(int32 Index, int32 Count)
	{
		return Count > 0 ? FMath::Clamp(Index, 0, Count - 1) : 0;
	}

	void SetPlayerHHVMaterialShownOnAllLods(USkeletalMeshComponent* Component, int32 MaterialIndex, bool bShow)
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

	FString MakeHHVMorphSafeMaterialName(const UMaterialInterface* Material)
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

	int32 ExtractPlayerHHVEyeNumber(const FUEHHVCustomizationOption& Option)
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

	int32 FindNearestHHVEyePaletteIndex(const TArray<FLinearColor>& Palette, const FLinearColor& Color)
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

	UTexture* LoadHHVEyeCompositeTexture(
		const FUEHHVCustomizationOption& Option,
		const FLinearColor& EyeColor,
		const TArray<FLinearColor>& EyePalette)
	{
		const int32 EyeNumber = ExtractPlayerHHVEyeNumber(Option);
		const int32 ColorIndex = FindNearestHHVEyePaletteIndex(EyePalette, EyeColor.GetClamped());
		const FString TextureName = FString::Printf(
			TEXT("T_Player_Eye%03d_Composite_C%02d"),
			EyeNumber,
			ColorIndex);
		const FString TexturePath = FString::Printf(
			TEXT("%s/Eye%03d/%s.%s"),
			HHVEyeCompositeFolder,
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
			HHVEyeCompositeFolder,
			EyeNumber,
			*FallbackName,
			*FallbackName);
		return LoadObject<UTexture>(nullptr, *FallbackPath);
	}

	void ApplyPlayerHHVEyeColorParameters(UMaterialInstanceDynamic* Material, const FLinearColor& EyeColor)
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

	void ApplyPlayerHHVEyeTextureParameters(UMaterialInstanceDynamic* Material, UTexture* Texture)
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

	void EnsurePlayerHHVSkeletalMaterialUsage(UMaterialInterface* Material)
	{
		if (!Material)
		{
			return;
		}

		// 추출 머티리얼 중 SkeletalMesh/MorphTargets 사용 플래그가 빠진 것이 있어,
		// 체형 모프 적용 뒤 회색 기본 머티리얼로 떨어지지 않게 적용 전에 확인한다.
		Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh);
		Material->CheckMaterialUsage_Concurrent(MATUSAGE_MorphTargets);
	}

	UMaterialInterface* LoadPlayerHHVMeshLocalMaterial(
		const USkeletalMesh* Mesh,
		const UMaterialInterface* CurrentMaterial,
		int32 MaterialIndex,
		int32 MaterialCount)
	{
		if (!Mesh)
		{
			return nullptr;
		}

		FString MeshObjectPath = Mesh->GetPathName();
		int32 DotIndex = INDEX_NONE;
		if (MeshObjectPath.FindChar(TEXT('.'), DotIndex))
		{
			MeshObjectPath.LeftInline(DotIndex);
		}

		const FString MeshFolder = FPaths::GetPath(MeshObjectPath);
		const bool bIsAssetsFbxOutfitMesh =
			MeshObjectPath.Contains(TEXT("/AssetsFBX/")) &&
			MeshObjectPath.Contains(TEXT("/Outfit/"));
		if (!MeshFolder.EndsWith(TEXT("/SkeletalMeshes")) && !bIsAssetsFbxOutfitMesh)
		{
			return nullptr;
		}

		const FString MeshOwnerFolder = FPaths::GetPath(MeshFolder);
		const FString MaterialName = CurrentMaterial ? CurrentMaterial->GetName() : FString();
		if (bIsAssetsFbxOutfitMesh)
		{
			// 의상 FBX 메쉬에는 이미 원본 /Assets/Pal 머티리얼 슬롯이 들어 있다.
			// 같은 이름의 /AssetsFBX 로컬 머티리얼은 추출 중 깨진 경우가 있어 덮어쓰지 않는다.
			return nullptr;
		}

		const auto LoadLocalMaterialByName = [&MeshOwnerFolder](const FString& CandidateName) -> UMaterialInterface*
		{
			if (CandidateName.IsEmpty())
			{
				return nullptr;
			}

			const FString LocalMaterialPath = FString::Printf(
				TEXT("%s/Materials/%s.%s"),
				*MeshOwnerFolder,
				*CandidateName,
				*CandidateName);
			return LoadObject<UMaterialInterface>(nullptr, *LocalMaterialPath);
		};
		const auto LoadSourceOutfitMaterialByName =
			[&MeshFolder](const FString& CandidateName, const FString& VersionFolder) -> UMaterialInterface*
		{
			if (CandidateName.IsEmpty() || VersionFolder.IsEmpty())
			{
				return nullptr;
			}

			const FString VariantFolder = FPaths::GetPath(MeshFolder);
			const FString OutfitRootFolder = FPaths::GetPath(VariantFolder);
			const FString SourceMaterialPath = FString::Printf(
				TEXT("%s/%s/%s.%s"),
				*OutfitRootFolder,
				*VersionFolder,
				*CandidateName,
				*CandidateName);
			return LoadObject<UMaterialInterface>(nullptr, *SourceMaterialPath);
		};

		FString MeshMaterialStem = Mesh->GetName();
		MeshMaterialStem.RemoveFromStart(TEXT("SK_"));
		if (MeshMaterialStem.EndsWith(TEXT("_2")))
		{
			MeshMaterialStem.LeftChopInline(2);
		}

		if (MeshMaterialStem.Contains(TEXT("Hair")))
		{
			// 헤어는 메쉬 기본 슬롯이 원본이다. 옷 보정 로직으로 덮으면 색과 윤곽이 틀어진다.
			return nullptr;
		}

		const bool bSingleOutfitSlotUsesBodyMaterial =
			MaterialIndex == 0 &&
			MaterialCount == 1 &&
			MaterialName.Contains(TEXT("Body")) &&
			Mesh->GetName().Contains(TEXT("Outfit"));
		if (bSingleOutfitSlotUsesBodyMaterial)
		{
			// 단일 슬롯 의상이 몸 피부 머티리얼을 물고 있으면 같은 의상 폴더의 원본 M01을 찾아 쓴다.
			FString OutfitMaterialStem = MeshMaterialStem;
			if (!OutfitMaterialStem.Contains(TEXT("_v")))
			{
				OutfitMaterialStem += TEXT("_v01");
			}
			FString VersionFolder = TEXT("v01");
			const int32 VersionIndex = OutfitMaterialStem.Find(TEXT("_v"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (VersionIndex != INDEX_NONE && OutfitMaterialStem.Len() >= VersionIndex + 4)
			{
				VersionFolder = OutfitMaterialStem.Mid(VersionIndex + 1, 3);
			}
			OutfitMaterialStem = TEXT("MI_") + OutfitMaterialStem;
			if (UMaterialInterface* OutfitMaterial = LoadSourceOutfitMaterialByName(
				OutfitMaterialStem + TEXT("_M01"),
				VersionFolder))
			{
				return OutfitMaterial;
			}
			if (UMaterialInterface* OutfitMaterial = LoadLocalMaterialByName(OutfitMaterialStem + TEXT("_M01")))
			{
				return OutfitMaterial;
			}
		}

		return nullptr;
	}

	void ApplyPlayerHHVOptionMaterials(
		USkeletalMeshComponent* Component,
		const FUEHHVCustomizationOption& Option,
		EUEHHVGender Gender)
	{
		if (!Component)
		{
			return;
		}

		return;
		const int32 MaterialCount = 0;
		/*
		const int32 MaterialCount = FMath::Min(Component->GetNumMaterials(), Materials.Num());
		for (int32 Index = 0; Index < MaterialCount; ++Index)
		{
			UMaterialInterface* Material = Materials[Index].Get();
			if (!Material)
			{
				continue;
			}

			// 게임 캐릭터도 카탈로그가 가진 원본 머티리얼만 슬롯 순서대로 적용한다.
			EnsurePlayerHHVSkeletalMaterialUsage(Material);
			Component->SetMaterial(Index, Material);
		}
		*/
	}

}

AUEPlayerCharacter::AUEPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 420.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	MovementSyncComponent = CreateDefaultSubobject<UUEPlayerMovementSyncComponent>(TEXT("MovementSyncComponent"));

	static ConstructorHelpers::FClassFinder<AUEPokemonCharacter> DefaultPokemonClass(TEXT("/Game/Pokemon/BP_Pokemon"));
	if (DefaultPokemonClass.Succeeded())
	{
		PokemonCompanionClass = DefaultPokemonClass.Class;
	}

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));

	HHVBodyEquipmentMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HHVBodyEquipmentMesh"));
	HHVBodyEquipmentMesh->SetupAttachment(GetMesh());
	HHVBodyEquipmentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HHVBodyEquipmentMesh->bReceivesDecals = false;

	HHVHeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HHVHeadMesh"));
	HHVHeadMesh->SetupAttachment(GetMesh());
	HHVHeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HHVHeadMesh->bReceivesDecals = false;

	HHVHairMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HHVHairMesh"));
	HHVHairMesh->SetupAttachment(GetMesh());
	HHVHairMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HHVHairMesh->bReceivesDecals = false;

	static ConstructorHelpers::FObjectFinder<UUEHHVCustomizationCatalog> CatalogFinder(
		TEXT("/Game/CharacterCustomization/HHV/Data/DA_HHVCustomizationCatalog"));
	if (CatalogFinder.Succeeded())
	{
		HHVCustomizationCatalog = CatalogFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UUEPlayerAnimationDataAsset> PlayerAnimationDataFinder(
		TEXT("/Game/Data/Animation/DA_PlayerAnimation"));
	if (PlayerAnimationDataFinder.Succeeded())
	{
		// 몽타주/시퀀스 참조는 데이터 에셋에 모아두고, 캐릭터는 그 에셋만 기본으로 잡는다.
		PlayerAnimationData = PlayerAnimationDataFinder.Object;
	}
}

void AUEPlayerCharacter::PlayerCharacterInit()
{
	// 플레이 시작 시 블루프린트 기본값이 덮여도 캐릭터 메쉬 방향은 C++ 기준으로 맞춘다.
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
}


void AUEPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	PlayerCharacterInit();
	ApplyPendingHHVAppearance();
	RefreshMovementSpeed();
	RegisterPokemonServerRoster();
}

void AUEPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PokemonDespawnTimerHandle);
	}

	if (IsValid(SpawnedPokemon))
	{
		NotifyPokemonServerDespawned(SpawnedPokemon.Get());
		NotifyPokemonWorldDespawned(SpawnedPokemon.Get());
		SpawnedPokemon->Destroy();
		SpawnedPokemon = nullptr;
	}

	PendingDespawnPokemon = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AUEPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyLocalMovementInput();
	UpdateHHVAnimation();
}

FVector AUEPlayerCharacter::GetDesiredMovementDirection() const
{
	return GetMoveDirectionFromInput(MovementInput, GetControlRotation());
}

bool AUEPlayerCharacter::IsPokemonCompanionSpawned() const
{
	return IsValid(SpawnedPokemon) && !bPokemonDespawnInProgress;
}

void AUEPlayerCharacter::SetRunning(bool bNewIsRunning)
{
	if (bIsRunning == bNewIsRunning)
	{
		return;
	}

	bIsRunning = bNewIsRunning;
	RefreshMovementSpeed();
}

void AUEPlayerCharacter::RefreshMovementSpeed()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = WalkSpeed * (bIsRunning ? RunSpeedMultiplier : 1.0f);
	}
}

void AUEPlayerCharacter::SetMovementInput(const FVector2D& NewMovementInput)
{
	MovementInput = NewMovementInput.GetClampedToMaxSize(1.0f);
}

void AUEPlayerCharacter::TogglePokemonCompanion()
{
	if (bPokemonDespawnInProgress)
	{
		return;
	}

	if (IsValid(SpawnedPokemon))
	{
		RequestDespawnPokemonCompanion();
		return;
	}

	TrySpawnPokemonCompanion();
}

void AUEPlayerCharacter::SetPokemonCompanionSpeciesData(UUEPokemonSpeciesData* NewSpeciesData)
{
	PokemonCompanionSpeciesData = NewSpeciesData;
	if (IsValid(SpawnedPokemon))
	{
		SpawnedPokemon->SetPokemonSpeciesData(PokemonCompanionSpeciesData);
	}
}

void AUEPlayerCharacter::SetSelectedPokemonCompanionInstanceId(int32 NewPokemonInstanceId)
{
	SelectedCompanionPokemonInstanceId = FMath::Max(NewPokemonInstanceId, 0);

	FUEPokemonServerOwnedPokemon OwnedPokemon;
	if (!IsValid(SpawnedPokemon)
		&& GetPokemonServerSubsystem()
		&& GetPokemonServerSubsystem()->TryGetOwnedPokemon(ServerPlayerId, SelectedCompanionPokemonInstanceId, OwnedPokemon))
	{
		PokemonCompanionSpeciesData = OwnedPokemon.SpeciesData;
	}
}

void AUEPlayerCharacter::ApplyLocalMovementInput()
{
	const FVector DesiredDirection = GetMoveDirectionFromInput(MovementInput, GetControlRotation());
	if (DesiredDirection.IsNearlyZero())
	{
		return;
	}

	// 이동 입력은 카메라 yaw 기준으로 계산하고, 대기 중 카메라 회전만으로 캐릭터를 돌리지는 않는다.
	const float InputStrength = FMath::Clamp(MovementInput.Size(), 0.0f, 1.0f);
	AddMovementInput(DesiredDirection, InputStrength);
}

FVector AUEPlayerCharacter::GetCameraForwardAxis(const FRotator& ViewRotation) const
{
	const FRotator CameraYawRotation(0.0f, ViewRotation.Yaw, 0.0f);
	return FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::X).GetSafeNormal2D();
}

FVector AUEPlayerCharacter::GetCameraRightAxis(const FRotator& ViewRotation) const
{
	const FRotator CameraYawRotation(0.0f, ViewRotation.Yaw, 0.0f);
	return FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::Y).GetSafeNormal2D();
}

FVector AUEPlayerCharacter::GetMoveDirectionFromInput(const FVector2D& Input, const FRotator& ViewRotation) const
{
	// 입력 벡터는 X=앞/뒤, Y=좌/우로 통일한다.
	const FVector Direction = GetCameraForwardAxis(ViewRotation) * Input.X + GetCameraRightAxis(ViewRotation) * Input.Y;
	return Direction.GetSafeNormal();
}

void AUEPlayerCharacter::ApplyServerMovementCorrection(const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bUseHardCorrection)
{
	const ETeleportType CorrectionTeleportType = bUseHardCorrection ? ETeleportType::TeleportPhysics : ETeleportType::None;

	// 서버 보정은 실제 위치와 속도를 맞춘 뒤 다음 틱에서 로컬 입력을 다시 적용한다.
	SetActorLocation(ServerPosition, false, nullptr, CorrectionTeleportType);
	SetActorRotation(ServerRotation, CorrectionTeleportType);
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->Velocity = ServerVelocity;
	}
}

bool AUEPlayerCharacter::TrySpawnPokemonCompanion()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FUEPokemonServerSpawnResponse SpawnResponse = RequestPokemonServerSpawn();
	if (!SpawnResponse.bAccepted)
	{
		return false;
	}
	PokemonCompanionSpeciesData = SpawnResponse.SpeciesData;

	TSubclassOf<AUEPokemonCharacter> ClassToSpawn = PokemonCompanionClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AUEPokemonCharacter::StaticClass();
	}

	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::OwnMode::Spawning);
	const HHV::PokemonAI::OwnContext Context = MakePokemonLifecycleContext(HHV::PokemonAI::RequestedAction::Spawn);
	const HHV::PokemonAI::Command SpawnCommand = PokemonLifecycleBrain.Tick(Context);
	if (SpawnCommand.Type != HHV::PokemonAI::CommandType::Spawn)
	{
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::OwnMode::NonCombat);
		ReleasePokemonServerSpawn(SpawnResponse);
		return false;
	}

	FVector SpawnLocation;
	FRotator SpawnRotation;
	if (!ResolvePokemonSpawnTransform(SpawnCommand, SpawnLocation, SpawnRotation))
	{
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::OwnMode::NonCombat);
		ReleasePokemonServerSpawn(SpawnResponse);
		return false;
	}

	BP_OnPokemonSpawnRequested(SpawnLocation, SpawnRotation);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AUEPokemonCharacter* NewPokemon = World->SpawnActor<AUEPokemonCharacter>(ClassToSpawn, SpawnLocation, SpawnRotation, SpawnParameters);
	if (!NewPokemon)
	{
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::OwnMode::NonCombat);
		ReleasePokemonServerSpawn(SpawnResponse);
		return false;
	}

	SpawnedPokemon = NewPokemon;
	NewPokemon->SetRenderType(EUEPokemonRenderType::Own);
	if (SpawnResponse.SpeciesData)
	{
		NewPokemon->SetPokemonSpeciesData(SpawnResponse.SpeciesData);
		NewPokemon->ApplyServerStats(SpawnResponse.CurrentHP, SpawnResponse.MaxHP);
	}
	if (UUEPokemonServerComponent* ServerComponent = NewPokemon->GetServerComponent())
	{
		ServerComponent->InitializeServerRuntimePokemon(
			SpawnResponse.RuntimePokemonId,
			SpawnResponse.PokemonInstanceId,
			SpawnResponse.CurrentHP,
			SpawnResponse.MaxHP
		);
		ServerComponent->SetFollowTargetActor(this);
		ServerComponent->SendServerAnimationEvent(
			EUEPokemonAnimationEvent::SpawnStarted,
			EUEPokemonAnimationState::Spawning,
			PokemonSpawnAnimationDuration
		);
	}
	if (UUEPokemonWorldSubsystem* PokemonWorldSubsystem = GetPokemonWorldSubsystem())
	{
		PokemonWorldSubsystem->RegisterExistingPokemon(NewPokemon, EUEPokemonRenderType::Own, ServerPlayerId);
	}

	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::OwnMode::NonCombat);
	BP_OnPokemonSpawned(NewPokemon);
	return true;
}

void AUEPlayerCharacter::RequestDespawnPokemonCompanion()
{
	if (!IsValid(SpawnedPokemon))
	{
		SpawnedPokemon = nullptr;
		return;
	}

	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::OwnMode::Despawning);
	const HHV::PokemonAI::OwnContext Context = MakePokemonLifecycleContext(HHV::PokemonAI::RequestedAction::Despawn);
	const HHV::PokemonAI::Command DespawnCommand = PokemonLifecycleBrain.Tick(Context);
	if (DespawnCommand.Type != HHV::PokemonAI::CommandType::Despawn)
	{
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::OwnMode::NonCombat);
		return;
	}

	PendingDespawnPokemon = SpawnedPokemon;
	bPokemonDespawnInProgress = true;
	if (UUEPokemonServerComponent* ServerComponent = PendingDespawnPokemon->GetServerComponent())
	{
		ServerComponent->SendServerAnimationEvent(
			EUEPokemonAnimationEvent::DespawnStarted,
			EUEPokemonAnimationState::Despawning,
			PokemonDespawnDelay
		);
	}
	BP_OnPokemonDespawnRequested(PendingDespawnPokemon.Get());

	if (PokemonDespawnDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PokemonDespawnTimerHandle, this, &ThisClass::FinishPokemonDespawn, PokemonDespawnDelay, false);
		return;
	}

	FinishPokemonDespawn();
}

void AUEPlayerCharacter::FinishPokemonDespawn()
{
	AUEPokemonCharacter* PokemonToDestroy = PendingDespawnPokemon.Get();
	if (!IsValid(PokemonToDestroy) && IsValid(SpawnedPokemon))
	{
		PokemonToDestroy = SpawnedPokemon.Get();
	}

	if (IsValid(PokemonToDestroy))
	{
		NotifyPokemonServerDespawned(PokemonToDestroy);
		NotifyPokemonWorldDespawned(PokemonToDestroy);
		PokemonToDestroy->Destroy();
	}

	if (SpawnedPokemon.Get() == PokemonToDestroy || !IsValid(SpawnedPokemon))
	{
		SpawnedPokemon = nullptr;
	}

	PendingDespawnPokemon = nullptr;
	bPokemonDespawnInProgress = false;
	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::OwnMode::NonCombat);
	BP_OnPokemonDespawned();
}

void AUEPlayerCharacter::RegisterPokemonServerRoster()
{
	UUEPokemonServerSubsystem* ServerSubsystem = GetPokemonServerSubsystem();
	if (!ServerSubsystem)
	{
		return;
	}

	ServerPlayerId = FMath::Max(ServerPlayerId, 1);

	TArray<FUEPokemonServerOwnedPokemon> OwnedPokemons = ServerOwnedPokemons;
	if (OwnedPokemons.IsEmpty())
	{
		FUEPokemonServerOwnedPokemon DefaultOwnedPokemon;
		DefaultOwnedPokemon.PokemonInstanceId = FMath::Max(SelectedCompanionPokemonInstanceId, 1);
		if (PokemonCompanionSpeciesData)
		{
			DefaultOwnedPokemon.SpeciesData = PokemonCompanionSpeciesData;
			DefaultOwnedPokemon.SpeciesId = PokemonCompanionSpeciesData->SpeciesId;
			DefaultOwnedPokemon.CurrentHP = PokemonCompanionSpeciesData->MaxHP;
		}
		else
		{
			TSubclassOf<AUEPokemonCharacter> ClassToInspect = PokemonCompanionClass;
			if (!ClassToInspect)
			{
				ClassToInspect = AUEPokemonCharacter::StaticClass();
			}

			const AUEPokemonCharacter* DefaultPokemon = ClassToInspect ? ClassToInspect->GetDefaultObject<AUEPokemonCharacter>() : nullptr;
			DefaultOwnedPokemon.SpeciesId = TEXT("Pikachu");
			DefaultOwnedPokemon.CurrentHP = DefaultPokemon ? DefaultPokemon->GetMaxHP() : 100.0f;
		}
		OwnedPokemons.Add(DefaultOwnedPokemon);
	}

	if (SelectedCompanionPokemonInstanceId <= 0 && !OwnedPokemons.IsEmpty())
	{
		SelectedCompanionPokemonInstanceId = OwnedPokemons[0].PokemonInstanceId > 0 ? OwnedPokemons[0].PokemonInstanceId : 1;
	}

	ServerSubsystem->RegisterOwnedPokemons(ServerPlayerId, OwnedPokemons);

	FUEPokemonServerOwnedPokemon SelectedOwnedPokemon;
	if (ServerSubsystem->TryGetOwnedPokemon(ServerPlayerId, SelectedCompanionPokemonInstanceId, SelectedOwnedPokemon))
	{
		PokemonCompanionSpeciesData = SelectedOwnedPokemon.SpeciesData;
	}
}

FUEPokemonServerSpawnResponse AUEPlayerCharacter::RequestPokemonServerSpawn()
{
	RegisterPokemonServerRoster();

	UUEPokemonServerSubsystem* ServerSubsystem = GetPokemonServerSubsystem();
	if (!ServerSubsystem)
	{
		FUEPokemonServerSpawnResponse Response;
		Response.Result = EUEPokemonServerSummonResult::InvalidPlayer;
		return Response;
	}

	return ServerSubsystem->RequestSpawnPokemon(ServerPlayerId, SelectedCompanionPokemonInstanceId);
}

void AUEPlayerCharacter::ReleasePokemonServerSpawn(const FUEPokemonServerSpawnResponse& SpawnResponse)
{
	if (!SpawnResponse.bAccepted)
	{
		return;
	}

	if (UUEPokemonServerSubsystem* ServerSubsystem = GetPokemonServerSubsystem())
	{
		ServerSubsystem->RequestDespawnPokemon(ServerPlayerId, SpawnResponse.RuntimePokemonId);
	}
}

void AUEPlayerCharacter::NotifyPokemonServerDespawned(AUEPokemonCharacter* PokemonToDestroy)
{
	if (!PokemonToDestroy)
	{
		return;
	}

	const UUEPokemonServerComponent* ServerComponent = PokemonToDestroy->GetServerComponent();
	const int32 RuntimePokemonId = ServerComponent ? ServerComponent->GetServerPokemonId() : PokemonToDestroy->GetServerPokemonId();
	if (RuntimePokemonId <= 0)
	{
		return;
	}

	if (UUEPokemonServerSubsystem* ServerSubsystem = GetPokemonServerSubsystem())
	{
		ServerSubsystem->RequestDespawnPokemon(ServerPlayerId, RuntimePokemonId);
	}
}

void AUEPlayerCharacter::NotifyPokemonWorldDespawned(AUEPokemonCharacter* PokemonToDestroy)
{
	if (!PokemonToDestroy)
	{
		return;
	}

	const UUEPokemonServerComponent* ServerComponent = PokemonToDestroy->GetServerComponent();
	const int32 RuntimePokemonId = ServerComponent ? ServerComponent->GetServerPokemonId() : PokemonToDestroy->GetServerPokemonId();
	if (RuntimePokemonId <= 0)
	{
		return;
	}

	if (UUEPokemonWorldSubsystem* PokemonWorldSubsystem = GetPokemonWorldSubsystem())
	{
		PokemonWorldSubsystem->DespawnPokemon(RuntimePokemonId, false);
	}
}

UUEPokemonServerSubsystem* AUEPlayerCharacter::GetPokemonServerSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UUEPokemonServerSubsystem>() : nullptr;
}

UUEPokemonWorldSubsystem* AUEPlayerCharacter::GetPokemonWorldSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UUEPokemonWorldSubsystem>() : nullptr;
}

HHV::PokemonAI::OwnContext AUEPlayerCharacter::MakePokemonLifecycleContext(HHV::PokemonAI::RequestedAction ActionRequest) const
{
	HHV::PokemonAI::OwnContext Context;
	Context.OwnerLocation = ToServerVec3(GetActorLocation());
	Context.OwnerYawDegrees = GetActorRotation().Yaw;
	Context.ActionRequest = ActionRequest;
	Context.Agent = MakePokemonAgentSettings();
	Context.PokemonLocation = IsValid(SpawnedPokemon)
		? ToServerVec3(SpawnedPokemon->GetActorLocation())
		: Context.OwnerLocation;
	return Context;
}

HHV::Map::AgentSettings AUEPlayerCharacter::MakePokemonAgentSettings() const
{
	HHV::Map::AgentSettings Agent;

	if (PokemonCompanionSpeciesData)
	{
		Agent.CapsuleRadius = PokemonCompanionSpeciesData->CapsuleRadius;
		Agent.CapsuleHalfHeight = PokemonCompanionSpeciesData->CapsuleHalfHeight;
		Agent.MaxStepHeight = PokemonCompanionSpeciesData->MaxStepHeight;
		Agent.WalkableFloorAngleDegrees = PokemonCompanionSpeciesData->WalkableFloorAngleDegrees;
		return Agent;
	}

	TSubclassOf<AUEPokemonCharacter> ClassToInspect = PokemonCompanionClass;
	if (!ClassToInspect)
	{
		ClassToInspect = AUEPokemonCharacter::StaticClass();
	}

	const AUEPokemonCharacter* DefaultPokemon = ClassToInspect ? ClassToInspect->GetDefaultObject<AUEPokemonCharacter>() : nullptr;
	if (!DefaultPokemon)
	{
		return Agent;
	}

	if (const UCapsuleComponent* DefaultCapsuleComponent = DefaultPokemon->GetCapsuleComponent())
	{
		Agent.CapsuleRadius = DefaultCapsuleComponent->GetScaledCapsuleRadius();
		Agent.CapsuleHalfHeight = DefaultCapsuleComponent->GetScaledCapsuleHalfHeight();
	}

	if (const UCharacterMovementComponent* MovementComponent = DefaultPokemon->GetCharacterMovement())
	{
		Agent.MaxStepHeight = MovementComponent->MaxStepHeight;
		Agent.WalkableFloorAngleDegrees = MovementComponent->GetWalkableFloorAngle();
	}

	return Agent;
}

bool AUEPlayerCharacter::ResolvePokemonSpawnTransform(const HHV::PokemonAI::Command& SpawnCommand, FVector& OutLocation, FRotator& OutRotation) const
{
	OutRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);

	for (const HHV::Map::Vec3& Candidate : SpawnCommand.PathPoints)
	{
		if (TryResolvePokemonSpawnCandidate(ToUnrealVector(Candidate), OutRotation, OutLocation))
		{
			return true;
		}
	}

	return TryResolvePokemonSpawnCandidate(ToUnrealVector(SpawnCommand.TargetLocation), OutRotation, OutLocation);
}

bool AUEPlayerCharacter::TryResolvePokemonSpawnCandidate(const FVector& CandidateLocation, const FRotator& SpawnRotation, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const HHV::Map::AgentSettings Agent = MakePokemonAgentSettings();
	FVector AdjustedLocation = CandidateLocation;

	FHitResult GroundHit;
	FCollisionQueryParams GroundTraceParams(TEXT("PokemonSpawnGroundTrace"), false, this);
	const FVector TraceStart = CandidateLocation + FVector(0.0f, 0.0f, PokemonSpawnGroundTraceDistance);
	const FVector TraceEnd = CandidateLocation - FVector(0.0f, 0.0f, PokemonSpawnGroundTraceDistance);
	if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundTraceParams))
	{
		AdjustedLocation.Z = GroundHit.ImpactPoint.Z + Agent.CapsuleHalfHeight;
	}

	const FCollisionShape SpawnShape = FCollisionShape::MakeCapsule(Agent.CapsuleRadius, Agent.CapsuleHalfHeight);
	const FCollisionQueryParams SpawnOverlapParams(TEXT("PokemonSpawnOverlap"), false);
	const bool bBlocked = World->OverlapBlockingTestByChannel(
		AdjustedLocation,
		SpawnRotation.Quaternion(),
		PokemonSpawnCollisionChannel,
		SpawnShape,
		SpawnOverlapParams
	);

	if (bBlocked)
	{
		return false;
	}

	OutLocation = AdjustedLocation;
	return true;
}

HHV::Map::Vec3 AUEPlayerCharacter::ToServerVec3(const FVector& Vector)
{
	return HHV::Map::Vec3{
		static_cast<float>(Vector.X),
		static_cast<float>(Vector.Y),
		static_cast<float>(Vector.Z)
	};
}

FVector AUEPlayerCharacter::ToUnrealVector(const HHV::Map::Vec3& Vector)
{
	return FVector(Vector.X, Vector.Y, Vector.Z);
}


void AUEPlayerCharacter::ApplyHHVAppearance(const FUEHHVAppearance& NewAppearance)
{
	if (!HHVCustomizationCatalog)
	{
		HHVCustomizationCatalog = LoadObject<UUEHHVCustomizationCatalog>(
			nullptr,
			TEXT("/Game/CharacterCustomization/HHV/Data/DA_HHVCustomizationCatalog.DA_HHVCustomizationCatalog"));
		if (!HHVCustomizationCatalog)
		{
			return;
		}
	}

	FUEHHVAppearance Appearance = NewAppearance;
	Appearance.BodyIndex = ClampOptionIndex(Appearance.BodyIndex, HHVCustomizationCatalog->GetOptionCount(EUEHHVCustomizationCategory::Body));
	if (Appearance.BodyIndex == 0 &&
		HHVCustomizationCatalog->GetOptionCount(EUEHHVCustomizationCategory::Body) > 2)
	{
		// 0번 Body는 예전 추출 기본값이라 실제 커마에서는 쓰지 않는다.
		// 게임 레벨로 넘어와도 커마 체형 타입은 TypeA=1, TypeB=2부터 시작한다.
		Appearance.BodyIndex = Appearance.Gender == EUEHHVGender::TypeB ? 2 : 1;
	}
	Appearance.HeadIndex = ClampOptionIndex(Appearance.HeadIndex, HHVCustomizationCatalog->GetOptionCount(EUEHHVCustomizationCategory::Head));
	Appearance.HairIndex = ClampOptionIndex(Appearance.HairIndex, HHVCustomizationCatalog->GetOptionCount(EUEHHVCustomizationCategory::Hair));
	Appearance.EyeIndex = ClampOptionIndex(Appearance.EyeIndex, HHVCustomizationCatalog->GetOptionCount(EUEHHVCustomizationCategory::Eyes));
	if (Appearance.HairColor.Equals(FLinearColor::White, 0.003f) && !HHVCustomizationCatalog->HairColors.IsEmpty())
	{
		// 저장값이 없는 예전 커마는 흰색을 "머리색 미지정"으로 들고 있어서, 카탈로그 기본 머리색으로 보정한다.
		Appearance.HairColor = HHVCustomizationCatalog->HairColors[0];
	}
	const int32 BodyEquipmentCount =
		HHVCustomizationCatalog->GetOptionCount(EUEHHVCustomizationCategory::BodyEquipment);
	if (BodyEquipmentCount > HHVFirstVisibleOutfitIndex)
	{
		// 의상 0번은 추출용 베이스라 제외하고, 실제 선택은 1~14번만 허용한다.
		const int32 LastVisibleOutfitIndex = FMath::Min(BodyEquipmentCount - 1, HHVMaxVisibleOutfits);
		Appearance.BodyEquipmentIndex = FMath::Clamp(
			Appearance.BodyEquipmentIndex,
			HHVFirstVisibleOutfitIndex,
			LastVisibleOutfitIndex);
	}
	else
	{
		Appearance.BodyEquipmentIndex = 0;
	}

	CurrentCustomizationGender = Appearance.Gender;

	const FUEHHVCustomizationOption& Body =
		HHVCustomizationCatalog->GetOption(EUEHHVCustomizationCategory::Body, Appearance.BodyIndex);
	const FUEHHVCustomizationOption& Outfit =
		HHVCustomizationCatalog->GetOption(EUEHHVCustomizationCategory::BodyEquipment, Appearance.BodyEquipmentIndex);
	const FUEHHVCustomizationOption& Head =
		HHVCustomizationCatalog->GetOption(EUEHHVCustomizationCategory::Head, Appearance.HeadIndex);
	const FUEHHVCustomizationOption& Hair =
		HHVCustomizationCatalog->GetOption(EUEHHVCustomizationCategory::Hair, Appearance.HairIndex);
	const FUEHHVCustomizationOption& Eyes =
		HHVCustomizationCatalog->GetOption(EUEHHVCustomizationCategory::Eyes, Appearance.EyeIndex);

	USkeletalMesh* BaseMesh = Body.LoadMesh(Appearance.Gender);
	USkeletalMesh* OutfitMesh = Outfit.LoadMesh(Appearance.Gender);
	const bool bSameOutfitAsBase =
		OutfitMesh == BaseMesh || GetPathNameSafe(OutfitMesh).Equals(GetPathNameSafe(BaseMesh));
	const bool bUsesSeparateOutfit = OutfitMesh && !bSameOutfitAsBase && Appearance.BodyEquipmentIndex > 0;
	// 팰월드 의상 메시는 피부/하의/신발을 포함한 완성형 플레이어 메쉬다.
	// 게임 플레이에서는 선택 의상 하나를 전신 리더로 사용해야 애니메이션의
	// 루트와 본 위치가 일치한다. 기본 바디를 리더로 두고 의상을 별도 포즈에
	// 얹으면 시작 직후 머리, 팔, 발이 서로 다른 원점으로 분해될 수 있다.
	USkeletalMesh* LeaderMesh = bUsesSeparateOutfit ? OutfitMesh : BaseMesh;
	UClass* PlayerAnimationBlueprint = LoadClass<UAnimInstance>(
		nullptr,
		TEXT("/Game/Data/Animation/HeavenHyperVoice/Player/ABP_UEAnimInstance.ABP_UEAnimInstance_C"));
	GetMesh()->SetSkeletalMesh(LeaderMesh);
	// 게임 레벨에서 커마 메쉬를 교체해도 플레이어 애님 블루프린트를 유지한다.
	// 메쉬 교체 뒤 SingleNode가 남으면 블렌드스페이스가 실행되지 않는다.
	if (PlayerAnimationBlueprint)
	{
		// 메쉬를 갈아끼운 직후에도 전신 애님 인스턴스를 새 스켈레톤에 맞춰 다시 만든다.
		// 이 초기화가 빠지면 이전 파츠의 애님 상태가 남아 머리만 움직일 수 있다.
		GetMesh()->SetEnableAnimation(true);
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint, true);
		GetMesh()->SetAnimInstanceClass(nullptr);
		GetMesh()->SetAnimInstanceClass(PlayerAnimationBlueprint);
		GetMesh()->ReinitializeAnimNodes();
	}
	GetMesh()->SetVisibility(true, false);
	GetMesh()->SetHiddenInGame(false, false);
	// 의상은 이미 전신 리더 메쉬로 들어갔으므로 별도 의상 컴포넌트는 비운다.
	// 같은 의상을 두 컴포넌트에 동시에 그리면 피부가 뚫리거나 파츠가 중복된다.
	HHVBodyEquipmentMesh->SetLeaderPoseComponent(nullptr);
	HHVBodyEquipmentMesh->SetSkeletalMesh(nullptr);
	HHVBodyEquipmentMesh->SetVisibility(false, true);
	HHVBodyEquipmentMesh->SetHiddenInGame(true, true);
	HHVHeadMesh->SetSkeletalMesh(Head.LoadMesh(Appearance.Gender));
	HHVHairMesh->SetSkeletalMesh(Hair.LoadMesh(Appearance.Gender));
	CurrentHHVAnimation = nullptr;

	ResetHHVMaterials(GetMesh());
	ResetHHVMaterials(HHVBodyEquipmentMesh);
	ResetHHVMaterials(HHVHeadMesh);
	ResetHHVMaterials(HHVHairMesh);
	// SetMaterial(nullptr) 뒤에는 메쉬 폴더의 원본 머티리얼을 다시 복원한다.
	// 이 단계가 빠지면 게임 레벨에서 의상/피부/머리카락이 흰색으로 나온다.
	ApplyHHVMeshLocalMaterials(GetMesh());
	ApplyHHVMeshLocalMaterials(HHVBodyEquipmentMesh);
	ApplyHHVMeshLocalMaterials(HHVHeadMesh);
	ApplyHHVMeshLocalMaterials(HHVHairMesh);

	// 메쉬를 교체할 때 이전 의상에서 숨긴 섹션 상태가 남지 않도록 먼저 모두 켠다.
	// 그 다음 아래의 의상/피부 규칙으로 필요한 섹션만 다시 숨긴다.
	const auto ResetHHVSectionVisibility = [](USkeletalMeshComponent* Component)
	{
		if (!Component)
		{
			return;
		}

		for (int32 MaterialIndex = 0; MaterialIndex < Component->GetNumMaterials(); ++MaterialIndex)
		{
			SetPlayerHHVMaterialShownOnAllLods(Component, MaterialIndex, true);
		}
	};
	ResetHHVSectionVisibility(GetMesh());
	ResetHHVSectionVisibility(HHVBodyEquipmentMesh);
	ResetHHVSectionVisibility(HHVHeadMesh);
	ResetHHVSectionVisibility(HHVHairMesh);

	HideHHVFaceCoverSections(GetMesh());
	HideHHVFaceCoverSections(HHVBodyEquipmentMesh);
	HideHHVFaceCoverSections(HHVHeadMesh);
	HideHHVFaceCoverSections(HHVHairMesh);

	for (USkeletalMeshComponent* Follower : {HHVBodyEquipmentMesh.Get(), HHVHeadMesh.Get(), HHVHairMesh.Get()})
	{
		const bool bCanShareLeaderPose =
			Follower &&
			Follower->GetSkeletalMeshAsset() &&
			GetMesh() &&
			GetMesh()->GetSkeletalMeshAsset() &&
			Follower->GetSkeletalMeshAsset()->GetSkeleton() == GetMesh()->GetSkeletalMeshAsset()->GetSkeleton();
		if (bCanShareLeaderPose)
		{
			// 동일 스켈레톤인 파츠만 리더 포즈를 공유한다.
			// 다른 스켈레톤에 강제로 연결하면 파츠가 원점에서 튀어나온다.
			Follower->SetEnableAnimation(false);
			Follower->SetLeaderPoseComponent(GetMesh(), true, true);
			Follower->SetComponentTickEnabled(true);
		}
		else if (Follower)
		{
			Follower->SetLeaderPoseComponent(nullptr);
			Follower->SetEnableAnimation(false);
			Follower->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Follower->bPauseAnims = true;
		}
	}
	HHVBodyEquipmentMesh->SetRelativeTransform(FTransform::Identity);
	HHVHeadMesh->SetRelativeTransform(FTransform::Identity);
	HHVHairMesh->SetRelativeTransform(FTransform::Identity);

	if (!Appearance.SkinColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyHHVColorToSlots(GetMesh(), Appearance.SkinColor, {TEXT("Body"), TEXT("Skin")});
		ApplyHHVColorToSlots(HHVBodyEquipmentMesh, Appearance.SkinColor, {TEXT("Body"), TEXT("Skin")});
		ApplyHHVColorToSlots(HHVHeadMesh, Appearance.SkinColor, {TEXT("Head"), TEXT("Skin")});
	}
	if (!Appearance.HairColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyHHVColorToSlots(HHVHairMesh, Appearance.HairColor, {TEXT("Hair")});
	}

	ApplyHHVEyeMaterial(HHVHeadMesh, Eyes, Appearance.EyeColor);
	ApplyHHVScale(Appearance);
	HideUnsupportedHHVAttachmentComponents();
}

void AUEPlayerCharacter::UpdateHHVAnimation()
{
	if (!GetMesh())
	{
		return;
	}

	if (!PlayerAnimationData)
	{
		PlayerAnimationData = LoadObject<UUEPlayerAnimationDataAsset>(
			nullptr,
			TEXT("/Game/Data/Animation/DA_PlayerAnimation.DA_PlayerAnimation"));
	}

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const float GroundSpeed = GetVelocity().Size2D();
	FGameplayTag AnimationTag = UEGameplayTags::State_Character_Idle;

	if (MovementComponent && MovementComponent->IsFalling())
	{
		AnimationTag = UEGameplayTags::State_Character_Fall;
	}
	else if (bIsRolling)
	{
		AnimationTag = UEGameplayTags::State_Character_Roll;
	}
	else if (GroundSpeed > 3.0f)
	{
		AnimationTag = bIsRunning
			? UEGameplayTags::State_Character_Run
			: UEGameplayTags::State_Character_Walk;
	}

	const UAnimSequence* SelectedAnimation =
		PlayerAnimationData->FindSequenceByTagForGender(AnimationTag, CurrentCustomizationGender);
	if (!SelectedAnimation)
	{
		SelectedAnimation = PlayerAnimationData->FindSequenceByTagForGender(
			UEGameplayTags::State_Character_Idle,
			CurrentCustomizationGender);
	}

	if (!SelectedAnimation)
	{
		// 데이터 에셋이 아직 예전 버전으로 저장돼 있어도 같은 원본 시퀀스를 찾는다.
		const bool bMale = CurrentCustomizationGender == EUEHHVGender::TypeB;
		const TCHAR* GenderFolder = bMale ? TEXT("Male") : TEXT("Female");
		FString SequenceName;
		if (AnimationTag == UEGameplayTags::State_Character_Walk)
		{
			SequenceName = bMale ? TEXT("AS_Player_Male_Walk") : TEXT("AS_Player_Female_Walk_None");
		}
		else if (AnimationTag == UEGameplayTags::State_Character_Run)
		{
			SequenceName = bMale ? TEXT("AS_Player_Male_Sprint") : TEXT("AS_Player_Female_Sprint");
		}
		else if (AnimationTag == UEGameplayTags::State_Character_Fall)
		{
			SequenceName = TEXT("AS_Player_Female_JumpDownLoop_None");
		}
		else if (AnimationTag == UEGameplayTags::State_Character_Roll)
		{
			SequenceName = TEXT("AS_Player_Female_RollFwd");
		}
		else
		{
			SequenceName = bMale ? TEXT("AS_Player_Male_Idle") : TEXT("AS_Player_Female_Idle_None");
		}

		const FString SequencePath = FString::Printf(
			TEXT("/Game/Data/Animation/HeavenHyperVoice/Player/%s/%s.%s"),
			GenderFolder,
			*SequenceName,
			*SequenceName);
		SelectedAnimation = LoadObject<UAnimSequence>(nullptr, *SequencePath);
	}

	const USkeletalMesh* CurrentMesh = GetMesh()->GetSkeletalMeshAsset();
	const auto MatchesCurrentSkeleton = [CurrentMesh](const UAnimSequence* Sequence)
	{
		return CurrentMesh &&
			CurrentMesh->GetSkeleton() &&
			Sequence &&
			Sequence->GetSkeleton() == CurrentMesh->GetSkeleton();
	};

	if (SelectedAnimation && SelectedAnimation != CurrentHHVAnimation)
	{
		if (MatchesCurrentSkeleton(SelectedAnimation))
		{
			PlayHHVAnimation(const_cast<UAnimSequence*>(SelectedAnimation), true);
		}
		else
		{
			// 다른 스켈레톤의 애니메이션은 루트 스케일/본 변환을 망가뜨릴 수 있다.
			// 호환되는 시퀀스를 연결하기 전에는 분해된 캐릭터를 만들지 않고
			// 현재 메쉬의 기준 포즈를 유지한다.
			GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			GetMesh()->bPauseAnims = true;
			CurrentHHVAnimation = nullptr;
		}
	}
}

void AUEPlayerCharacter::PlayHHVAnimation(UAnimSequence* Sequence, bool bLoop)
{
	if (!GetMesh() || !Sequence)
	{
		return;
	}

	// 현재 추출 애니메이션은 본체 스켈레톤 기준으로 검증된 시퀀스다.
	// 본체만 직접 재생하고, 의상/얼굴/머리는 아래의 리더 포즈를 공유한다.
	GetMesh()->SetEnableAnimation(true);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode, true);
	GetMesh()->PlayAnimation(Sequence, bLoop);
	CurrentHHVAnimation = Sequence;
}

void AUEPlayerCharacter::PlayHHVAnimationOnComponent(
	USkeletalMeshComponent* Component,
	UAnimSequence* Sequence,
	bool bLoop) const
{
	if (!Component || !Sequence || Component == GetMesh())
	{
		return;
	}

	// 리더 포즈를 쓸 수 없는 별도 파츠를 명시적으로 재생해야 할 때의 호환용 함수다.
	Component->SetEnableAnimation(true);
	Component->SetAnimationMode(EAnimationMode::AnimationSingleNode, true);
	Component->PlayAnimation(Sequence, bLoop);
}

void AUEPlayerCharacter::ApplyPendingHHVAppearance()
{
	FUEHHVAppearance PendingAppearance;
	UUEGameInstance* UEGameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!UEGameInstance || !UEGameInstance->GetPendingHHVAppearance(PendingAppearance))
	{
		// 저장된 커마가 없으면 다른 레벨에서도 기본 착장을 반드시 입힌다.
		ApplyHHVAppearance(FUEHHVAppearance());
		return;
	}

	ApplyHHVAppearance(PendingAppearance);
}

void AUEPlayerCharacter::ResetHHVMaterials(USkeletalMeshComponent* Component) const
{
	if (!Component)
	{
		return;
	}

	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		Component->SetMaterial(Index, nullptr);
		EnsurePlayerHHVSkeletalMaterialUsage(Component->GetMaterial(Index));
	}
}

void AUEPlayerCharacter::ApplyHHVMeshLocalMaterials(USkeletalMeshComponent* Component) const
{
	if (!Component || !Component->GetSkeletalMeshAsset())
	{
		return;
	}

	USkeletalMesh* SkeletalMeshAsset = Component->GetSkeletalMeshAsset();
	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* CurrentMaterial = Component->GetMaterial(Index);
		EnsurePlayerHHVSkeletalMaterialUsage(CurrentMaterial);
		if (UMaterialInterface* LocalMaterial = LoadPlayerHHVMeshLocalMaterial(
			SkeletalMeshAsset,
			CurrentMaterial,
			Index,
			Component->GetNumMaterials()))
		{
			// 같은 메쉬 폴더의 원본 머티리얼이 있으면 그쪽을 우선 사용한다.
			EnsurePlayerHHVSkeletalMaterialUsage(LocalMaterial);
			Component->SetMaterial(Index, LocalMaterial);
		}
	}
}

void AUEPlayerCharacter::ApplyHHVMorphSafeMaterials(USkeletalMeshComponent* Component) const
{
	if (!Component)
	{
		return;
	}

	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* OriginalMaterial = Component->GetMaterial(Index);
		const FString SafeName = MakeHHVMorphSafeMaterialName(OriginalMaterial);
		const FString SafePath = FString::Printf(
			TEXT("%s/%s.%s"),
			HHVMorphSafeMaterialFolder,
			*SafeName,
			*SafeName);
		UMaterialInterface* SafeMaterial = LoadObject<UMaterialInterface>(nullptr, *SafePath);
		if (SafeMaterial)
		{
			// 원본 머티리얼을 부모로 둔 안전한 머티리얼을 사용해, 체형 모프 후에도 원본 텍스처를 유지한다.
			Component->SetMaterial(Index, SafeMaterial);
		}
	}
}

void AUEPlayerCharacter::ApplyHHVColorToSlots(
	USkeletalMeshComponent* Component,
	const FLinearColor& Color,
	const TArray<FString>& SlotContains) const
{
	if (!Component || Color.Equals(FLinearColor::White, 0.003f))
	{
		return;
	}

	const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		const FString SlotName = SlotNames.IsValidIndex(Index) ? SlotNames[Index].ToString() : FString();
		const FString SlotIdentity = SlotName.ToLower();
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

		bool bMatches = false;
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
			for (const FName ParameterName : {TEXT("TintColor"), TEXT("Color"), TEXT("BaseColor"), TEXT("Base Color"), TEXT("MainColor"), TEXT("HairColor"), TEXT("Hair Color")})
			{
				DynamicMaterial->SetVectorParameterValue(ParameterName, Color);
			}
		}
	}
}

void AUEPlayerCharacter::ApplyHHVEyeMaterial(
	USkeletalMeshComponent* Component,
	const FUEHHVCustomizationOption& EyeOption,
	const FLinearColor& EyeColor) const
{
	if (!Component || !EyeOption.Material)
	{
		return;
	}

	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		if (IsHHVEyeMaterialSlot(Component, Index))
		{
			EnsurePlayerHHVSkeletalMaterialUsage(EyeOption.Material);
			UMaterialInstanceDynamic* DynamicMaterial = Component->CreateDynamicMaterialInstance(Index, EyeOption.Material);
			if (!DynamicMaterial)
			{
				Component->SetMaterial(Index, EyeOption.Material);
				continue;
			}

			if (UTexture* CompositeTexture = LoadHHVEyeCompositeTexture(
				EyeOption,
				EyeColor,
				HHVCustomizationCatalog ? HHVCustomizationCatalog->EyeColors : TArray<FLinearColor>()))
			{
				// 흰자까지 포함된 합성 텍스처를 써서 눈 색만 바꾸고 얼굴 머티리얼은 건드리지 않는다.
				// 합성 텍스처가 있으면 흰자까지 보존된 원본 텍스처를 그대로 쓴다.
				ApplyPlayerHHVEyeTextureParameters(DynamicMaterial, CompositeTexture);
				continue;
			}
			ApplyPlayerHHVEyeColorParameters(DynamicMaterial, EyeColor);
		}
	}
}

bool AUEPlayerCharacter::IsHHVEyeMaterialSlot(USkeletalMeshComponent* Component, int32 MaterialIndex) const
{
	if (!Component || MaterialIndex < 0 || MaterialIndex >= Component->GetNumMaterials())
	{
		return false;
	}

	const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
	const FString SlotName = SlotNames.IsValidIndex(MaterialIndex) ? SlotNames[MaterialIndex].ToString().ToLower() : FString();
	const USkeletalMesh* SkeletalMeshAsset = Component->GetSkeletalMeshAsset();
	const TArray<FSkeletalMaterial>* AssetMaterials = SkeletalMeshAsset ? &SkeletalMeshAsset->GetMaterials() : nullptr;
	const FSkeletalMaterial* AssetMaterial =
		AssetMaterials && AssetMaterials->IsValidIndex(MaterialIndex) ? &(*AssetMaterials)[MaterialIndex] : nullptr;
	const FString AssetSlotName = AssetMaterial ? AssetMaterial->MaterialSlotName.ToString().ToLower() : FString();
	const UMaterialInterface* DefaultMaterial = AssetMaterial ? AssetMaterial->MaterialInterface : nullptr;
	const FString MaterialIdentity = DefaultMaterial ? DefaultMaterial->GetName().ToLower() : FString();
	const FString SlotIdentity = SlotName + TEXT(" ") + AssetSlotName;
	const FString Combined = SlotIdentity + TEXT(" ") + MaterialIdentity;

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
	const bool bHasHHVEyeMaterial =
		MaterialIdentity.Contains(TEXT("mi_player_eye")) ||
		MaterialIdentity.Contains(TEXT("player_eye")) ||
		MaterialIdentity.Contains(TEXT("iris")) ||
		MaterialIdentity.Contains(TEXT("pupil"));
	return bLooksLikeEyeSlot && bHasHHVEyeMaterial;
}

void AUEPlayerCharacter::HideHHVFaceCoverSections(USkeletalMeshComponent* Component) const
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
		const FString MaterialPath = GetPathNameSafe(Component->GetMaterial(MaterialIndex)).ToLower();
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

		SetPlayerHHVMaterialShownOnAllLods(Component, MaterialIndex, false);
	}
}

void AUEPlayerCharacter::HideHHVBaseBodyOutfitSections(USkeletalMeshComponent* Component) const
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
		// 폴더 경로의 "Outfit" 문자열은 피부 머티리얼에도 포함될 수 있다.
		// 실제 슬롯/머티리얼 이름만 보고 옷 섹션을 판별해야 베이스 피부를 숨기지 않는다.
		const FString MaterialName = FPaths::GetBaseFilename(MaterialPath).ToLower();
		const FString Identity = Slot + TEXT(" ") + MaterialName;
		const bool bIsSkinSection =
			Identity.Contains(TEXT("body")) ||
			Identity.Contains(TEXT("skin")) ||
			Identity.Contains(TEXT("player_female_body")) ||
			Identity.Contains(TEXT("player_male_body"));
		const bool bIsOutfitSection =
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

		// 실제 플레이 캐릭터도 기본 옷 슬롯만 끄고, 피부/팔/다리는 의상 빈 부분을 메우도록 남긴다.
		SetPlayerHHVMaterialShownOnAllLods(Component, MaterialIndex, false);
	}
}

void AUEPlayerCharacter::HideHHVEquipmentSkinSections(USkeletalMeshComponent* Component) const
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
		if (!bIsSkinSection)
		{
			continue;
		}

		// 피부는 베이스 몸에서만 렌더링하고, 선택 의상은 옷/장비 섹션만 렌더링한다.
		SetPlayerHHVMaterialShownOnAllLods(Component, MaterialIndex, false);
	}
}

void AUEPlayerCharacter::HideUnsupportedHHVAttachmentComponents() const
{
	TArray<USkeletalMeshComponent*> Components;
	GetComponents(Components);

	for (USkeletalMeshComponent* Component : Components)
	{
		if (!Component ||
			Component == GetMesh() ||
			Component == HHVBodyEquipmentMesh ||
			Component == HHVHeadMesh ||
			Component == HHVHairMesh)
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

		// 현재 커마는 부착물을 쓰지 않으므로, 이전 BP 컴포넌트가 남아 있으면 게임 레벨에서 숨긴다.
		Component->SetLeaderPoseComponent(nullptr);
		Component->SetSkeletalMesh(nullptr);
		Component->SetVisibility(false, true);
		Component->SetHiddenInGame(true, true);
	}
}

void AUEPlayerCharacter::ApplyHHVScale(const FUEHHVAppearance& /*NewAppearance*/) const
{
	// 커마 체격 스케일 기능은 제거했다. 저장된 예전 값이 있어도 항상 기본 체형으로 되돌린다.
	GetMesh()->SetRelativeScale3D(FVector::OneVector);
	HHVBodyEquipmentMesh->SetRelativeScale3D(FVector::OneVector);
	HHVHeadMesh->SetRelativeScale3D(FVector::OneVector);
	HHVHairMesh->SetRelativeScale3D(FVector::OneVector);

	ApplyPlayerHHVSignedMorphTarget(GetMesh(), TEXT("BS_Torso_min"), TEXT("BS_Torso_max"), 0.0f);
	ApplyPlayerHHVSignedMorphTarget(GetMesh(), TEXT("BS_Arm_min"), TEXT("BS_Arm_max"), 0.0f);
	ApplyPlayerHHVSignedMorphTarget(GetMesh(), TEXT("BS_Leg_min"), TEXT("BS_Leg_max"), 0.0f);
	ApplyPlayerHHVSignedMorphTarget(HHVBodyEquipmentMesh, TEXT("BS_Torso_min"), TEXT("BS_Torso_max"), 0.0f);
	ApplyPlayerHHVSignedMorphTarget(HHVBodyEquipmentMesh, TEXT("BS_Arm_min"), TEXT("BS_Arm_max"), 0.0f);
	ApplyPlayerHHVSignedMorphTarget(HHVBodyEquipmentMesh, TEXT("BS_Leg_min"), TEXT("BS_Leg_max"), 0.0f);
}
