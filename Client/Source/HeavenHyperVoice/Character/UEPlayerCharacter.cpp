#include "UEPlayerCharacter.h"

#include "../CharacterCustomization/Palworld/Data/UEPalworldCustomizationTypes.h"
#include "../Component/UEPlayerMovementSyncComponent.h"
#include "../Data/UEPlayerAnimationDataAsset.h"
#include "../Pokemon/UEPokemonCharacter.h"
#include "../Pokemon/UEPokemonTestServerComponent.h"
#include "../System/UEGameInstance.h"

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
	const TCHAR* PalworldMorphSafeMaterialFolder = TEXT("/Game/CharacterCustomization/Palworld/Generated/MorphSafeMaterials");
	const TCHAR* PalworldEyeCompositeFolder = TEXT("/Game/CharacterCustomization/Palworld/Generated/EyeComposite");
	constexpr int32 PalworldMaxVisibleOutfits = 14;
	constexpr int32 PalworldFirstVisibleOutfitIndex = 1;

	void ApplyPlayerPalworldSignedMorphTarget(USkeletalMeshComponent* Component, const FName MinTarget, const FName MaxTarget, float Value)
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

	void SetPlayerPalworldMaterialShownOnAllLods(USkeletalMeshComponent* Component, int32 MaterialIndex, bool bShow)
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

	FString MakePalworldMorphSafeMaterialName(const UMaterialInterface* Material)
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

	int32 ExtractPlayerPalworldEyeNumber(const FUEPalworldCustomizationOption& Option)
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

	int32 FindNearestPalworldEyePaletteIndex(const TArray<FLinearColor>& Palette, const FLinearColor& Color)
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

	UTexture* LoadPalworldEyeCompositeTexture(
		const FUEPalworldCustomizationOption& Option,
		const FLinearColor& EyeColor,
		const TArray<FLinearColor>& EyePalette)
	{
		const int32 EyeNumber = ExtractPlayerPalworldEyeNumber(Option);
		const int32 ColorIndex = FindNearestPalworldEyePaletteIndex(EyePalette, EyeColor.GetClamped());
		const FString TextureName = FString::Printf(
			TEXT("T_Player_Eye%03d_Composite_C%02d"),
			EyeNumber,
			ColorIndex);
		const FString TexturePath = FString::Printf(
			TEXT("%s/Eye%03d/%s.%s"),
			PalworldEyeCompositeFolder,
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
			PalworldEyeCompositeFolder,
			EyeNumber,
			*FallbackName,
			*FallbackName);
		return LoadObject<UTexture>(nullptr, *FallbackPath);
	}

	void ApplyPlayerPalworldEyeColorParameters(UMaterialInstanceDynamic* Material, const FLinearColor& EyeColor)
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

	void ApplyPlayerPalworldEyeTextureParameters(UMaterialInstanceDynamic* Material, UTexture* Texture)
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

	void EnsurePlayerPalworldSkeletalMaterialUsage(UMaterialInterface* Material)
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

	UMaterialInterface* LoadPlayerPalworldMeshLocalMaterial(
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
			MeshObjectPath.Contains(TEXT("/Palworld/AssetsFBX/")) &&
			MeshObjectPath.Contains(TEXT("/Outfit/"));
		if (!MeshFolder.EndsWith(TEXT("/SkeletalMeshes")) && !bIsAssetsFbxOutfitMesh)
		{
			return nullptr;
		}

		const FString MeshOwnerFolder = FPaths::GetPath(MeshFolder);
		const FString MaterialName = CurrentMaterial ? CurrentMaterial->GetName() : FString();
		if (bIsAssetsFbxOutfitMesh)
		{
			const auto LoadAssetsFbxLocalMaterialByName =
				[&MeshFolder](const FString& CandidateName) -> UMaterialInterface*
			{
				if (CandidateName.IsEmpty())
				{
					return nullptr;
				}

				const FString LocalMaterialPath = FString::Printf(
					TEXT("%s/%s.%s"),
					*MeshFolder,
					*CandidateName,
					*CandidateName);
				return LoadObject<UMaterialInterface>(nullptr, *LocalMaterialPath);
			};
			if (UMaterialInterface* LocalMaterial = LoadAssetsFbxLocalMaterialByName(MaterialName))
			{
				// AssetsFBX 의상 슬롯이 다른 버전 MI를 가리킬 때가 있어, 같은 메쉬 폴더의 원본 MI를 우선 사용한다.
				return LocalMaterial;
			}

			const FString MeshNameLower = Mesh->GetName().ToLower();
			const FString MaterialPathLower = GetPathNameSafe(CurrentMaterial).ToLower();
			const bool bMeshIsFemale = MeshNameLower.Contains(TEXT("female"));
			const bool bMeshIsMale = !bMeshIsFemale && MeshNameLower.Contains(TEXT("male"));
			const bool bMaterialIsFemale = MaterialPathLower.Contains(TEXT("_female_"));
			const bool bMaterialIsMale = MaterialPathLower.Contains(TEXT("_male_"));
			const bool bWrongGenderMaterial =
				(bMeshIsFemale && bMaterialIsMale) ||
				(bMeshIsMale && bMaterialIsFemale);
			const int32 SuffixIndex = MaterialName.Find(TEXT("_M"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (bWrongGenderMaterial && SuffixIndex != INDEX_NONE && MaterialName.Len() >= SuffixIndex + 4)
			{
				const FString MaterialSuffix = MaterialName.Mid(SuffixIndex + 1, 3);
				const FString LocalMaterialName = TEXT("MI___") + MaterialSuffix;
				const FString LocalMaterialPath = FString::Printf(
					TEXT("%s/%s.%s"),
					*MeshFolder,
					*LocalMaterialName,
					*LocalMaterialName);
				// AssetsFBX 의상 일부는 여성 메쉬 슬롯에 남성 M03 머티리얼이 꽂혀 있다.
				// 같은 메쉬 폴더의 MI___M## 머티리얼이 있으면 그 추출 머티리얼로 교체한다.
				return LoadObject<UMaterialInterface>(nullptr, *LocalMaterialPath);
			}
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
			// 헤어는 추출된 메쉬 슬롯의 원본 머티리얼을 그대로 사용한다.
			// 다른 파트 보정 로직이 헤어 머티리얼을 덮으면 색과 윤곽이 틀어진다.
			return nullptr;
		}

		const bool bSingleOutfitSlotUsesBodyMaterial =
			MaterialIndex == 0 &&
			MaterialCount == 1 &&
			MaterialName.Contains(TEXT("Body")) &&
			Mesh->GetName().Contains(TEXT("Outfit"));
		if (bSingleOutfitSlotUsesBodyMaterial)
		{
			// 일부 추출 의상은 메쉬 슬롯이 하나인데 기본 머티리얼이 몸 피부로 들어와 있다.
			// 이때만 Palworld 원본 /Outfit/.../v##/MI_*_M01 머티리얼을 찾아 입힌다.
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

		// 이미 메쉬 슬롯에 원본 Palworld 머티리얼이 있으면 그대로 둔다.
		return nullptr;
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

	PalworldBodyEquipmentMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PalworldBodyEquipmentMesh"));
	PalworldBodyEquipmentMesh->SetupAttachment(GetMesh());
	PalworldBodyEquipmentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PalworldBodyEquipmentMesh->bReceivesDecals = false;

	PalworldHeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PalworldHeadMesh"));
	PalworldHeadMesh->SetupAttachment(GetMesh());
	PalworldHeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PalworldHeadMesh->bReceivesDecals = false;

	PalworldHairMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PalworldHairMesh"));
	PalworldHairMesh->SetupAttachment(GetMesh());
	PalworldHairMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PalworldHairMesh->bReceivesDecals = false;

	static ConstructorHelpers::FObjectFinder<UUEPalworldCustomizationCatalog> CatalogFinder(
		TEXT("/Game/CharacterCustomization/Palworld/Data/DA_PalworldCustomizationCatalog"));
	if (CatalogFinder.Succeeded())
	{
		PalworldCustomizationCatalog = CatalogFinder.Object;
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
	RefreshMovementSpeed();
	ApplyPendingPalworldAppearance();
}

void AUEPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PokemonDespawnTimerHandle);
	}

	if (IsValid(SpawnedPokemon))
	{
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

	TSubclassOf<AUEPokemonCharacter> ClassToSpawn = PokemonCompanionClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AUEPokemonCharacter::StaticClass();
	}

	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::Spawning);
	const HHV::PokemonAI::CompanionContext Context = MakePokemonLifecycleContext(HHV::PokemonAI::RequestedAction::Spawn);
	const HHV::PokemonAI::Command SpawnCommand = PokemonLifecycleBrain.Tick(Context);
	if (SpawnCommand.Type != HHV::PokemonAI::CommandType::Spawn)
	{
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
		return false;
	}

	FVector SpawnLocation;
	FRotator SpawnRotation;
	if (!ResolvePokemonSpawnTransform(SpawnCommand, SpawnLocation, SpawnRotation))
	{
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
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
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
		return false;
	}

	SpawnedPokemon = NewPokemon;
	if (UUEPokemonTestServerComponent* TestServerComponent = NewPokemon->GetTestServerComponent())
	{
		TestServerComponent->SetFollowTargetActor(this);
	}

	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
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

	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::Despawning);
	const HHV::PokemonAI::CompanionContext Context = MakePokemonLifecycleContext(HHV::PokemonAI::RequestedAction::Despawn);
	const HHV::PokemonAI::Command DespawnCommand = PokemonLifecycleBrain.Tick(Context);
	if (DespawnCommand.Type != HHV::PokemonAI::CommandType::Despawn)
	{
		PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
		return;
	}

	PendingDespawnPokemon = SpawnedPokemon;
	bPokemonDespawnInProgress = true;
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
		PokemonToDestroy->Destroy();
	}

	if (SpawnedPokemon.Get() == PokemonToDestroy || !IsValid(SpawnedPokemon))
	{
		SpawnedPokemon = nullptr;
	}

	PendingDespawnPokemon = nullptr;
	bPokemonDespawnInProgress = false;
	PokemonLifecycleBrain.SetMode(HHV::PokemonAI::CompanionMode::NonCombat);
	BP_OnPokemonDespawned();
}

HHV::PokemonAI::CompanionContext AUEPlayerCharacter::MakePokemonLifecycleContext(HHV::PokemonAI::RequestedAction ActionRequest) const
{
	HHV::PokemonAI::CompanionContext Context;
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


void AUEPlayerCharacter::ApplyPalworldAppearance(const FUEPalworldAppearance& NewAppearance)
{
	if (!PalworldCustomizationCatalog)
	{
		PalworldCustomizationCatalog = LoadObject<UUEPalworldCustomizationCatalog>(
			nullptr,
			TEXT("/Game/CharacterCustomization/Palworld/Data/DA_PalworldCustomizationCatalog.DA_PalworldCustomizationCatalog"));
		if (!PalworldCustomizationCatalog)
		{
			return;
		}
	}

	FUEPalworldAppearance Appearance = NewAppearance;
	Appearance.BodyIndex = ClampOptionIndex(Appearance.BodyIndex, PalworldCustomizationCatalog->GetOptionCount(EUEPalworldCustomizationCategory::Body));
	if (Appearance.BodyIndex == 0 &&
		PalworldCustomizationCatalog->GetOptionCount(EUEPalworldCustomizationCategory::Body) > 2)
	{
		// 0번 Body는 예전 추출 기본값이라 실제 커마에서는 쓰지 않는다.
		// 게임 레벨로 넘어와도 Palworld 체형 타입은 TypeA=1, TypeB=2부터 시작한다.
		Appearance.BodyIndex = Appearance.Gender == EUEPalworldGender::TypeB ? 2 : 1;
	}
	Appearance.HeadIndex = ClampOptionIndex(Appearance.HeadIndex, PalworldCustomizationCatalog->GetOptionCount(EUEPalworldCustomizationCategory::Head));
	Appearance.HairIndex = ClampOptionIndex(Appearance.HairIndex, PalworldCustomizationCatalog->GetOptionCount(EUEPalworldCustomizationCategory::Hair));
	Appearance.EyeIndex = ClampOptionIndex(Appearance.EyeIndex, PalworldCustomizationCatalog->GetOptionCount(EUEPalworldCustomizationCategory::Eyes));
	const int32 BodyEquipmentCount =
		PalworldCustomizationCatalog->GetOptionCount(EUEPalworldCustomizationCategory::BodyEquipment);
	if (BodyEquipmentCount > PalworldFirstVisibleOutfitIndex)
	{
		// 의상 0번은 추출용 베이스라 제외하고, 실제 선택은 1~14번만 허용한다.
		const int32 LastVisibleOutfitIndex = FMath::Min(BodyEquipmentCount - 1, PalworldMaxVisibleOutfits);
		Appearance.BodyEquipmentIndex = FMath::Clamp(
			Appearance.BodyEquipmentIndex,
			PalworldFirstVisibleOutfitIndex,
			LastVisibleOutfitIndex);
	}
	else
	{
		Appearance.BodyEquipmentIndex = 0;
	}

	const FUEPalworldCustomizationOption& Body =
		PalworldCustomizationCatalog->GetOption(EUEPalworldCustomizationCategory::Body, Appearance.BodyIndex);
	const FUEPalworldCustomizationOption& Outfit =
		PalworldCustomizationCatalog->GetOption(EUEPalworldCustomizationCategory::BodyEquipment, Appearance.BodyEquipmentIndex);
	const FUEPalworldCustomizationOption& Head =
		PalworldCustomizationCatalog->GetOption(EUEPalworldCustomizationCategory::Head, Appearance.HeadIndex);
	const FUEPalworldCustomizationOption& Hair =
		PalworldCustomizationCatalog->GetOption(EUEPalworldCustomizationCategory::Hair, Appearance.HairIndex);
	const FUEPalworldCustomizationOption& Eyes =
		PalworldCustomizationCatalog->GetOption(EUEPalworldCustomizationCategory::Eyes, Appearance.EyeIndex);

	USkeletalMesh* BaseMesh = Body.LoadMesh(Appearance.Gender);
	USkeletalMesh* OutfitMesh = Outfit.LoadMesh(Appearance.Gender);
	const bool bSameOutfitAsBase =
		OutfitMesh == BaseMesh || GetPathNameSafe(OutfitMesh).Equals(GetPathNameSafe(BaseMesh));
	const bool bUsesSeparateOutfit = OutfitMesh && !bSameOutfitAsBase && Appearance.BodyEquipmentIndex > 0;
	GetMesh()->SetSkeletalMesh(BaseMesh);
	PalworldBodyEquipmentMesh->SetSkeletalMesh(bUsesSeparateOutfit ? OutfitMesh : nullptr);
	PalworldBodyEquipmentMesh->SetVisibility(bUsesSeparateOutfit, true);
	PalworldBodyEquipmentMesh->SetHiddenInGame(!bUsesSeparateOutfit, true);
	PalworldHeadMesh->SetSkeletalMesh(Head.LoadMesh(Appearance.Gender));
	PalworldHairMesh->SetSkeletalMesh(Hair.LoadMesh(Appearance.Gender));

	ResetPalworldMaterials(GetMesh());
	ApplyPalworldMeshLocalMaterials(GetMesh());
	// 원본 Palworld 바디/의상 텍스처를 그대로 써야 하므로 대체 머티리얼을 덮지 않는다.
	ResetPalworldMaterials(PalworldBodyEquipmentMesh);
	ApplyPalworldMeshLocalMaterials(PalworldBodyEquipmentMesh);
	// 별도 의상도 SkeletalMesh에 저장된 원본 머티리얼 슬롯을 그대로 사용한다.
	ResetPalworldMaterials(PalworldHeadMesh);
	ApplyPalworldMeshLocalMaterials(PalworldHeadMesh);
	ResetPalworldMaterials(PalworldHairMesh);
	ApplyPalworldMeshLocalMaterials(PalworldHairMesh);
	HidePalworldFaceCoverSections(GetMesh());
	if (bUsesSeparateOutfit)
	{
		// 기본 몸은 유지하고 기본 OldCloth 섹션만 숨긴 뒤, 선택 의상을 별도 메쉬로 얹는다.
		HidePalworldBaseBodyOutfitSections(GetMesh());
	}
	HidePalworldFaceCoverSections(PalworldBodyEquipmentMesh);
	HidePalworldFaceCoverSections(PalworldHeadMesh);
	HidePalworldFaceCoverSections(PalworldHairMesh);

	const USkeleton* LeaderSkeleton = GetMesh() && GetMesh()->GetSkeletalMeshAsset()
		? GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()
		: nullptr;
	for (USkeletalMeshComponent* Follower : {PalworldBodyEquipmentMesh.Get(), PalworldHeadMesh.Get(), PalworldHairMesh.Get()})
	{
		USkeletalMesh* FollowerMesh = Follower ? Follower->GetSkeletalMeshAsset() : nullptr;
		const USkeleton* FollowerSkeleton = FollowerMesh ? FollowerMesh->GetSkeleton() : nullptr;
		if (FollowerMesh && LeaderSkeleton && FollowerSkeleton == LeaderSkeleton)
		{
			// 같은 Palworld 스켈레톤을 쓰는 파트만 애니메이션 포즈를 공유한다.
			Follower->SetLeaderPoseComponent(GetMesh(), true, false);
		}
		else if (Follower)
		{
			Follower->SetLeaderPoseComponent(nullptr);
		}
	}
	PalworldBodyEquipmentMesh->SetRelativeTransform(FTransform::Identity);
	PalworldHeadMesh->SetRelativeTransform(FTransform::Identity);
	PalworldHairMesh->SetRelativeTransform(FTransform::Identity);

	if (!Appearance.SkinColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyPalworldColorToSlots(GetMesh(), Appearance.SkinColor, {TEXT("Body"), TEXT("Skin")});
		ApplyPalworldColorToSlots(PalworldBodyEquipmentMesh, Appearance.SkinColor, {TEXT("Body"), TEXT("Skin")});
		ApplyPalworldColorToSlots(PalworldHeadMesh, Appearance.SkinColor, {TEXT("Head"), TEXT("Skin")});
	}
	if (!Appearance.HairColor.Equals(FLinearColor::White, 0.003f))
	{
		ApplyPalworldColorToSlots(PalworldHairMesh, Appearance.HairColor, {TEXT("Hair")});
	}

	ApplyPalworldEyeMaterial(PalworldHeadMesh, Eyes, Appearance.EyeColor);
	ApplyPalworldScale(Appearance);
	HideUnsupportedPalworldAttachmentComponents();
}

void AUEPlayerCharacter::ApplyPendingPalworldAppearance()
{
	FUEPalworldAppearance PendingAppearance;
	UUEGameInstance* UEGameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!UEGameInstance || !UEGameInstance->GetPendingPalworldAppearance(PendingAppearance))
	{
		// 저장된 커마가 없으면 다른 레벨에서도 Palworld 기본 착장을 반드시 입힌다.
		ApplyPalworldAppearance(FUEPalworldAppearance());
		return;
	}

	ApplyPalworldAppearance(PendingAppearance);
}

void AUEPlayerCharacter::ResetPalworldMaterials(USkeletalMeshComponent* Component) const
{
	if (!Component)
	{
		return;
	}

	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		Component->SetMaterial(Index, nullptr);
		EnsurePlayerPalworldSkeletalMaterialUsage(Component->GetMaterial(Index));
	}
}

void AUEPlayerCharacter::ApplyPalworldMeshLocalMaterials(USkeletalMeshComponent* Component) const
{
	if (!Component || !Component->GetSkeletalMeshAsset())
	{
		return;
	}

	USkeletalMesh* SkeletalMeshAsset = Component->GetSkeletalMeshAsset();
	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* CurrentMaterial = Component->GetMaterial(Index);
		EnsurePlayerPalworldSkeletalMaterialUsage(CurrentMaterial);
		if (UMaterialInterface* LocalMaterial = LoadPlayerPalworldMeshLocalMaterial(
			SkeletalMeshAsset,
			CurrentMaterial,
			Index,
			Component->GetNumMaterials()))
		{
			// 일부 추출 메시가 다른 성별 폴더의 머티리얼을 물고 있어서,
			// 같은 메시 폴더 안에 복사된 원본 머티리얼이 있으면 그쪽을 우선 사용한다.
			EnsurePlayerPalworldSkeletalMaterialUsage(LocalMaterial);
			Component->SetMaterial(Index, LocalMaterial);
		}
	}
}

void AUEPlayerCharacter::ApplyPalworldMorphSafeMaterials(USkeletalMeshComponent* Component) const
{
	if (!Component)
	{
		return;
	}

	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* OriginalMaterial = Component->GetMaterial(Index);
		const FString SafeName = MakePalworldMorphSafeMaterialName(OriginalMaterial);
		const FString SafePath = FString::Printf(
			TEXT("%s/%s.%s"),
			PalworldMorphSafeMaterialFolder,
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

void AUEPlayerCharacter::ApplyPalworldColorToSlots(
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
			DynamicMaterial->SetVectorParameterValue(TEXT("TintColor"), Color);
		}
	}
}

void AUEPlayerCharacter::ApplyPalworldEyeMaterial(
	USkeletalMeshComponent* Component,
	const FUEPalworldCustomizationOption& EyeOption,
	const FLinearColor& EyeColor) const
{
	if (!Component || !EyeOption.Material)
	{
		return;
	}

	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		if (IsPalworldEyeMaterialSlot(Component, Index))
		{
			EnsurePlayerPalworldSkeletalMaterialUsage(EyeOption.Material);
			UMaterialInstanceDynamic* DynamicMaterial = Component->CreateDynamicMaterialInstance(Index, EyeOption.Material);
			if (!DynamicMaterial)
			{
				Component->SetMaterial(Index, EyeOption.Material);
				continue;
			}

			if (UTexture* CompositeTexture = LoadPalworldEyeCompositeTexture(
				EyeOption,
				EyeColor,
				PalworldCustomizationCatalog ? PalworldCustomizationCatalog->EyeColors : TArray<FLinearColor>()))
			{
				// 흰자까지 포함된 합성 텍스처를 써서 눈 색만 바꾸고 얼굴 머티리얼은 건드리지 않는다.
				// 합성 텍스처가 있으면 흰자까지 보존된 원본 텍스처를 그대로 쓴다.
				ApplyPlayerPalworldEyeTextureParameters(DynamicMaterial, CompositeTexture);
				continue;
			}
			ApplyPlayerPalworldEyeColorParameters(DynamicMaterial, EyeColor);
		}
	}
}

bool AUEPlayerCharacter::IsPalworldEyeMaterialSlot(USkeletalMeshComponent* Component, int32 MaterialIndex) const
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
	const bool bHasPalworldEyeMaterial =
		MaterialIdentity.Contains(TEXT("mi_player_eye")) ||
		MaterialIdentity.Contains(TEXT("player_eye")) ||
		MaterialIdentity.Contains(TEXT("iris")) ||
		MaterialIdentity.Contains(TEXT("pupil"));
	return bLooksLikeEyeSlot && bHasPalworldEyeMaterial;
}

void AUEPlayerCharacter::HidePalworldFaceCoverSections(USkeletalMeshComponent* Component) const
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

		SetPlayerPalworldMaterialShownOnAllLods(Component, MaterialIndex, false);
	}
}

void AUEPlayerCharacter::HidePalworldBaseBodyOutfitSections(USkeletalMeshComponent* Component) const
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

		// 실제 게임 레벨에서 기본 OldCloth가 선택 의상과 겹치지 않도록
		// 베이스 바디의 옷 섹션만 끄고, 피부/팔/목 섹션은 유지한다.
		SetPlayerPalworldMaterialShownOnAllLods(Component, MaterialIndex, false);
	}
}

void AUEPlayerCharacter::HideUnsupportedPalworldAttachmentComponents() const
{
	TArray<USkeletalMeshComponent*> Components;
	GetComponents(Components);

	for (USkeletalMeshComponent* Component : Components)
	{
		if (!Component ||
			Component == GetMesh() ||
			Component == PalworldBodyEquipmentMesh ||
			Component == PalworldHeadMesh ||
			Component == PalworldHairMesh)
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

		// 현재 Palworld 커마는 부착물을 쓰지 않으므로, 이전 BP 컴포넌트가 남아 있으면 게임 레벨에서 숨긴다.
		Component->SetLeaderPoseComponent(nullptr);
		Component->SetSkeletalMesh(nullptr);
		Component->SetVisibility(false, true);
		Component->SetHiddenInGame(true, true);
	}
}

void AUEPlayerCharacter::ApplyPalworldScale(const FUEPalworldAppearance& NewAppearance) const
{
	GetMesh()->SetRelativeScale3D(FVector::OneVector);
	PalworldBodyEquipmentMesh->SetRelativeScale3D(FVector::OneVector);
	PalworldHeadMesh->SetRelativeScale3D(FVector::OneVector);
	PalworldHairMesh->SetRelativeScale3D(FVector::OneVector);

	// Palworld 원본 체형 모프만 사용한다. 머리와 루트 스케일은 따로 건드리지 않는다.
	ApplyPlayerPalworldSignedMorphTarget(GetMesh(), TEXT("BS_Torso_min"), TEXT("BS_Torso_max"), NewAppearance.TorsoVolume);
	ApplyPlayerPalworldSignedMorphTarget(GetMesh(), TEXT("BS_Arm_min"), TEXT("BS_Arm_max"), NewAppearance.ArmVolume);
	ApplyPlayerPalworldSignedMorphTarget(GetMesh(), TEXT("BS_Leg_min"), TEXT("BS_Leg_max"), NewAppearance.LegVolume);
	ApplyPlayerPalworldSignedMorphTarget(PalworldBodyEquipmentMesh, TEXT("BS_Torso_min"), TEXT("BS_Torso_max"), NewAppearance.TorsoVolume);
	ApplyPlayerPalworldSignedMorphTarget(PalworldBodyEquipmentMesh, TEXT("BS_Arm_min"), TEXT("BS_Arm_max"), NewAppearance.ArmVolume);
	ApplyPlayerPalworldSignedMorphTarget(PalworldBodyEquipmentMesh, TEXT("BS_Leg_min"), TEXT("BS_Leg_max"), NewAppearance.LegVolume);
}
