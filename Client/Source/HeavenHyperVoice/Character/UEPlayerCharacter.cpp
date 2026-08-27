#include "UEPlayerCharacter.h"

#include "../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"
#include "../Animation/UEAnimInstance.h"
#include "../Animation/UEFollowerAnimInstance.h"
#include "../Player/Server/UEPlayerMovementSyncComponent.h"
#include "../Data/UEPlayerAnimationDataAsset.h"
#include "../Pokemon/UEPokemonCharacter.h"
#include "../Pokemon/UEPokemonSpeciesCatalog.h"
#include "../Pokemon/UEPokemonSpeciesData.h"
#include "../Pokemon/Server/UEPokemonServerComponent.h"
#include "../Pokemon/Server/UEPokemonServerSubsystem.h"
#include "../Pokemon/Effects/UEPokemonSummonEffectComponent.h"
#include "../Pokemon/UEPokemonWorldSubsystem.h"
#include "../System/UEGameInstance.h"
#include "../UEGameplayTags.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshRenderData.h"

namespace
{
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
	GetCharacterMovement()->bNotifyApex = true;
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
	RefreshCharacterState();

	if (bIsRemoteProxy)
	{
		// 남의 캐릭터다. 겉모습만 맞추고 끝낸다 — 로컬 포켓몬 서버 로스터에
		// 남을 끼워 넣거나, 남 몫의 동행을 부르거나, QA 를 또 돌리면 안 된다.
		ConfigureRemoteProxyMovement();
		return;
	}

	RegisterPokemonServerRoster();

	// 로스터가 등록됐으니 바로 동행 포켓몬을 부른다. 입력 키(TogglePokemonCompanion)
	// 로도 껐다 켤 수 있다.
	if (bAutoSpawnPokemonCompanion && !IsValid(SpawnedPokemon))
	{
		TrySpawnPokemonCompanion();
	}

	// QA 하네스는 셋업이 다 끝난 뒤에 돈다.
	StartGameplayQAIfRequested();
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

	if (bIsRemoteProxy)
	{
		// 입력이 없다. 좌표는 스냅샷이 지시하고 여기서는 따라가기만 한다.
		// 구르기도 그 플레이어 클라가 판정해 좌표로 넘어온다.
		UpdateRemoteProxyMovement(DeltaSeconds);
		RefreshCharacterState();
		return;
	}

	else if (!bIsRolling)
	{
		ApplyLocalMovementInput();
	}
	RefreshCharacterState();
}

void AUEPlayerCharacter::MakeRemoteProxy()
{
	bIsRemoteProxy = true;

	// 컨트롤러가 붙으면 남의 캐릭터가 내 입력을 먹는다.
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	// 동행은 그 플레이어의 클라가 부른다. 여기서 부르면 남의 파트너가 내
	// 로컬 시뮬레이션으로 하나 더 생긴다.
	bAutoSpawnPokemonCompanion = false;

	// 카메라는 하나면 된다. 스프링암은 매 프레임 충돌을 훑으므로 꺼둔다.
	if (CameraBoom)
	{
		CameraBoom->SetComponentTickEnabled(false);
	}
	if (FollowCamera)
	{
		FollowCamera->SetActive(false);
	}

	// 필드 연결은 내 것 하나뿐이어야 한다. 이 복제본의 싱크 컴포넌트는
	// BeginPlay 에서 IsRemoteProxy() 를 보고 스스로 물러난다.
}

void AUEPlayerCharacter::ConfigureRemoteProxyMovement()
{
	// 좌표를 서버가 전부 지시하므로 로컬 이동 시뮬레이션은 끈다. 켜 두면
	// 중력과 보간이 서로 밀며 캐릭터가 떨린다. 야생 포켓몬과 같은 설정이다.
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->GravityScale = 0.0f;
		MovementComponent->SetMovementMode(MOVE_None);
		MovementComponent->SetComponentTickEnabled(false);
	}

	// 밀어내기는 서버가 판정한다. 복제본끼리 부딪혀 밀리면 좌표만 어긋난다.
	SetActorEnableCollision(false);
}

void AUEPlayerCharacter::ApplyRemoteMoveTarget(const FVector& TargetLocation,
	const FRotator& TargetRotation, bool bTeleported)
{
	// 로코모션 애니메이션이 속도를 읽는다. 서버는 속도를 안 보내니 목표가
	// 갱신된 간격으로 만들어 넣는다. 스냅샷이 20Hz 고정이라 그 값으로 나눈다.
	constexpr float kSnapshotInterval = 1.0f / 20.0f;
	RemoteVelocity = (TargetLocation - GetActorLocation()) / kSnapshotInterval;

	RemoteTargetLocation = TargetLocation;
	RemoteTargetRotation = TargetRotation;
	bHasRemoteTarget = true;

	// 순간이동이거나 너무 벌어졌으면 보간을 포기하고 붙인다. 시야에 막 들어온
	// 경우가 여기다 — 안 그러면 맵 반대편에서 걸어오는 것처럼 보인다.
	if (bTeleported || FVector::Dist(GetActorLocation(), TargetLocation) >= RemoteHardSnapDistance)
	{
		SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		SetActorRotation(TargetRotation, ETeleportType::TeleportPhysics);
		RemoteVelocity = FVector::ZeroVector;
		bHasRemoteTarget = false;
	}
}

void AUEPlayerCharacter::UpdateRemoteProxyMovement(float DeltaSeconds)
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->Velocity = RemoteVelocity;
	}

	if (!bHasRemoteTarget)
	{
		RemoteVelocity = FVector::ZeroVector;
		return;
	}

	const FVector NewLocation =
		FMath::VInterpTo(GetActorLocation(), RemoteTargetLocation, DeltaSeconds, RemoteInterpSpeed);
	const FRotator NewRotation =
		FMath::RInterpTo(GetActorRotation(), RemoteTargetRotation, DeltaSeconds, RemoteRotationInterpSpeed);

	SetActorLocation(NewLocation, false);
	SetActorRotation(NewRotation);

	if (FVector::DistSquared(NewLocation, RemoteTargetLocation) <= 1.0f)
	{
		SetActorLocation(RemoteTargetLocation, false);
		bHasRemoteTarget = false;
	}
}

void AUEPlayerCharacter::Jump()
{
	if (ActionStateTag.IsValid() || bIsRolling)
	{
		return;
	}

	CancelLanding();
	Super::Jump();
	CharacterStateTag = UEGameplayTags::State_Character_Jump;
}

void AUEPlayerCharacter::NotifyJumpApex()
{
	Super::NotifyJumpApex();
	CharacterStateTag = UEGameplayTags::State_Character_Fall;
}

void AUEPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// 구르기용 전방 발사가 바닥에 닿을 때 별도 착지 상태로 덮어쓰지 않는다.
	if (bIsRolling)
	{
		return;
	}

	// 이동하거나 다른 행동 중이면 착지 모션을 거치지 않고 현재 상태로 바로 이어간다.
	if (!MovementInput.IsNearlyZero() || ActionStateTag.IsValid())
	{
		CancelLanding();
		RefreshCharacterState();
		return;
	}

	bLandingStateActive = true;
	CharacterStateTag = UEGameplayTags::State_Character_Landing;
	GetWorldTimerManager().ClearTimer(LandingStateTimerHandle);
	GetWorldTimerManager().SetTimer(
		LandingStateTimerHandle,
		this,
		&ThisClass::FinishLanding,
		LandingStateDuration,
		false);
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

void AUEPlayerCharacter::SetCharacterActionState(const FGameplayTag& NewStateTag)
{
	const bool bSupportedAction =
		NewStateTag == UEGameplayTags::State_Character_Holding ||
		NewStateTag == UEGameplayTags::State_Character_Throw ||
		NewStateTag == UEGameplayTags::State_Character_Damage ||
		NewStateTag == UEGameplayTags::State_Character_Death;
	if (!bSupportedAction)
	{
		return;
	}

	CancelLanding();
	ActionStateTag = NewStateTag;
	RefreshCharacterState();
}

void AUEPlayerCharacter::ClearCharacterActionState()
{
	ActionStateTag = FGameplayTag::EmptyTag;
	RefreshCharacterState();
}

void AUEPlayerCharacter::RefreshCharacterState()
{
	if (ActionStateTag.IsValid())
	{
		CharacterStateTag = ActionStateTag;
		return;
	}

	if (bIsRolling)
	{
		CharacterStateTag = UEGameplayTags::State_Character_Roll;
		return;
	}

	if (bLandingStateActive)
	{
		CharacterStateTag = UEGameplayTags::State_Character_Landing;
		return;
	}

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (MovementComponent && MovementComponent->IsFalling())
	{
		CharacterStateTag = GetVelocity().Z > 0.0f
			? UEGameplayTags::State_Character_Jump
			: UEGameplayTags::State_Character_Fall;
		return;
	}

	if (GetVelocity().SizeSquared2D() > 9.0f)
	{
		CharacterStateTag = bIsRunning
			? UEGameplayTags::State_Character_Run
			: UEGameplayTags::State_Character_Walk;
		return;
	}

	CharacterStateTag = UEGameplayTags::State_Character_Idle;
}

void AUEPlayerCharacter::FinishRoll()
{
	bIsRolling = false;
	RefreshCharacterState();
}

void AUEPlayerCharacter::FinishLanding()
{
	CancelLanding();
	RefreshCharacterState();
}

void AUEPlayerCharacter::CancelLanding()
{
	if (!bLandingStateActive)
	{
		return;
	}

	bLandingStateActive = false;
	GetWorldTimerManager().ClearTimer(LandingStateTimerHandle);
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
	if (bLandingStateActive && !MovementInput.IsNearlyZero())
	{
		// 착지 중 이동 입력이 들어오면 착지 끝을 기다리지 않고 로코모션으로 보간한다.
		CancelLanding();
		RefreshCharacterState();
	}
}

void AUEPlayerCharacter::TogglePokemonCompanion()
{
	if (bPokemonDespawnInProgress)
	{
		return;
	}

	if (IsValid(SpawnedPokemon))
	{
		// R 키는 플레이어 모션을 재생하지 않는다. 포켓몬 쪽 연출 컴포넌트가
		// 포켓몬의 빛을 플레이어 몸으로 되돌린 뒤 안전하게 파괴한다.
		RequestDespawnPokemonCompanion();
		return;
	}

	// 볼을 던지는 동작 대신 플레이어 몸에서 소환 위치로 빛이 이동한다.
	// 자동 소환도 같은 TrySpawn 경로를 사용하므로 입력/자동 소환의 화면 규칙이 같다.
	TrySpawnPokemonCompanion();
}

bool AUEPlayerCharacter::CommandPokemonAttack(int32 AttackSlot)
{
	if (!IsPokemonCompanionSpawned())
	{
		// 보유 포켓몬이 필드에 나오기 전에는 공격 입력을 서버로 보내지 않는다.
		return false;
	}

	EUEPokemonAttackAnimation AttackAnimation = EUEPokemonAttackAnimation::None;
	switch (AttackSlot)
	{
	case 1:
		AttackAnimation = EUEPokemonAttackAnimation::Attack01;
		break;
	case 2:
		AttackAnimation = EUEPokemonAttackAnimation::Attack02;
		break;
	case 3:
		AttackAnimation = EUEPokemonAttackAnimation::RangeAttack01;
		break;
	case 4:
		AttackAnimation = EUEPokemonAttackAnimation::RangeAttack02;
		break;
	default:
		return false;
	}

	UUEPokemonServerComponent* ServerComponent = SpawnedPokemon->GetServerComponent();
	if (!ServerComponent)
	{
		return false;
	}

	// 지금은 숫자키 예시지만 이후 STT가 기술 슬롯을 찾으면 이 함수부터 같은 경로를 재사용한다.
	return ServerComponent->SendServerAttackCommand(AttackAnimation);
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

	// 절대 두 마리를 만들지 않는다. 이 함수가 어느 경로로 다시 불려도(자동 소환,
	// 입력 키, 서버 재등록) 이미 동행 중이면 그대로 둔다. 이 가드가 없으면
	// 호출될 때마다 SpawnedPokemon 을 덮어써 이전 액터가 유령으로 겹쳐 쌓인다.
	if (IsValid(SpawnedPokemon))
	{
		UE_LOG(LogTemp, Warning, TEXT("TrySpawnPokemonCompanion: 이미 동행 중, 중복 소환 무시"));
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("TrySpawnPokemonCompanion: 새 동행 소환"));

	// 스폰할 클래스를 먼저 정한다. 서버 자리를 잡아둔 뒤에 실패하면 그걸 도로
	// 반납해야 하는데, 여기서 끝내면 그럴 일이 없다.
	TSubclassOf<AUEPokemonCharacter> ClassToSpawn = PokemonCompanionClass;
	if (!ClassToSpawn)
	{
		// 예전에는 여기서 AUEPokemonCharacter::StaticClass() 로 넘어갔다. 그건
		// 메시도 큐브도 없는 맨 C++ 클래스라, **보이지 않는** 동행이 조용히
		// 스폰되고 함수는 true 를 반환했다. 동행이 안 보이는데 로그 한 줄
		// 남지 않던 원인이다. 못 만들면 못 만들었다고 말한다.
		UE_LOG(LogTemp, Error, TEXT("TrySpawnPokemonCompanion: 스폰할 포켓몬 클래스가 없다"));
		return false;
	}

	const FUEPokemonServerSpawnResponse SpawnResponse = RequestPokemonServerSpawn();
	if (!SpawnResponse.bAccepted)
	{
		return false;
	}
	PokemonCompanionSpeciesData = SpawnResponse.SpeciesData;

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
	float SpawnDuration = PokemonSpawnAnimationDuration;
	if (UUEPokemonSummonEffectComponent* SummonEffect = NewPokemon->GetSummonEffectComponent())
	{
		// 빛의 시작점은 포켓몬 Owner를 추측하지 않고 이 플레이어로 명시한다.
		// 모듈형 몸 메시의 spine_03을 찾아 베지어 곡선 P0로 사용한다.
		SummonEffect->SetEffectBodyActor(this);
		// 메시와 종별 크기가 모두 적용된 뒤 시작해야 입자와 발광 크기가 포켓몬에 맞는다.
		// 반환된 실제 시간을 서버 애니메이션 상태에도 넘겨 화면 연출과 상태 종료를 맞춘다.
		SpawnDuration = SummonEffect->PlaySpawnEffect(PokemonSpawnAnimationDuration);
	}
	// R 입력 자체가 아니라 실제 소환 성공 지점에서 재생해야 스폰 실패나 귀환 때 울지 않는다.
	// 종족 DataAsset의 SummonCry가 비어 있으면 아무 소리도 내지 않고 안전하게 넘어간다.
	NewPokemon->PlaySummonCry();
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
			SpawnDuration
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
	float DespawnDuration = PokemonDespawnDelay;
	if (UUEPokemonSummonEffectComponent* SummonEffect = PendingDespawnPokemon->GetSummonEffectComponent())
	{
		// 귀환 곡선의 마지막 점 P3도 이 플레이어의 현재 가슴 위치로 고정한다.
		SummonEffect->SetEffectBodyActor(this);
		// 설정값이 0이어도 컴포넌트 기본 시간으로 연출하고, 반환 시간을 파괴 타이머와 맞춘다.
		DespawnDuration = SummonEffect->PlayDespawnEffect(PokemonDespawnDelay);
	}
	if (UUEPokemonServerComponent* ServerComponent = PendingDespawnPokemon->GetServerComponent())
	{
		ServerComponent->SendServerAnimationEvent(
			EUEPokemonAnimationEvent::DespawnStarted,
			EUEPokemonAnimationState::Despawning,
			DespawnDuration
		);
	}
	BP_OnPokemonDespawnRequested(PendingDespawnPokemon.Get());

	if (DespawnDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PokemonDespawnTimerHandle, this, &ThisClass::FinishPokemonDespawn, DespawnDuration, false);
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

	// 동행 종족 데이터가 비어 있으면 카탈로그에서 기본 종족을 끌어온다. 이게
	// 없으면 동행은 종족 없는(SpeciesId=None) 큐브로 뜬다. 카탈로그가 아직
	// 비어 있으면 그대로 예전 동작이다.
	if (!PokemonCompanionSpeciesData && DefaultCompanionSpeciesId > 0)
	{
		if (PokemonSpeciesCatalog)
		{
			PokemonCompanionSpeciesData = PokemonSpeciesCatalog->Find(DefaultCompanionSpeciesId);
		}
	}

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
			// 종족 데이터가 없을 때 특정 포켓몬 이름을 임의로 넣지 않는다.
			DefaultOwnedPokemon.SpeciesId = NAME_None;
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
		UE_LOG(LogTemp, Error,
			TEXT("HHVCustomizationCatalog is not assigned in the PlayerCharacter Blueprint defaults."));
		return;
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
	// 애니메이션 기준 메시와 체형은 항상 Body 메시가 담당한다.
	// 선택 의상으로 기준 메시를 교체하면 의상에 없는 다리와 발이 사라진다.
	USkeletalMesh* LeaderMesh = BaseMesh;
	const TSubclassOf<UAnimInstance> PlayerAnimationBlueprint =
		Appearance.Gender == EUEHHVGender::TypeA
			? TypeAAnimationClass
			: TypeBAnimationClass;
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
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->SetVisibility(LeaderMesh != nullptr && !bUsesSeparateOutfit, false);
	GetMesh()->SetHiddenInGame(LeaderMesh == nullptr || bUsesSeparateOutfit, false);

	HHVBodyEquipmentMesh->SetLeaderPoseComponent(nullptr);
	HHVBodyEquipmentMesh->SetSkeletalMesh(bUsesSeparateOutfit ? OutfitMesh : nullptr);
	HHVBodyEquipmentMesh->SetVisibility(bUsesSeparateOutfit, true);
	HHVBodyEquipmentMesh->SetHiddenInGame(!bUsesSeparateOutfit, true);
	HHVHeadMesh->SetSkeletalMesh(Head.LoadMesh(Appearance.Gender));
	HHVHairMesh->SetSkeletalMesh(Hair.LoadMesh(Appearance.Gender));

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
		if (Follower && Follower->GetSkeletalMeshAsset() && GetMesh()->GetSkeletalMeshAsset())
		{
			// 원본 Player BP의 Head/Hair AnimBP처럼 부착 부모의 포즈를 본 이름으로 복사한다.
			Follower->SetLeaderPoseComponent(nullptr);
			Follower->SetEnableAnimation(true);
			Follower->SetAnimationMode(EAnimationMode::AnimationBlueprint, true);
			Follower->SetAnimInstanceClass(UUEFollowerAnimInstance::StaticClass());
			Follower->ReinitializeAnimNodes();
			if (UUEFollowerAnimInstance* FollowerInstance = Cast<UUEFollowerAnimInstance>(Follower->GetAnimInstance()))
			{
				FollowerInstance->SetCopyCurves(Follower == HHVHeadMesh);
				FollowerInstance->SetUseTargetEyeReferencePose(Follower == HHVHeadMesh);
			}
			Follower->SetComponentTickEnabled(true);
		}
		else if (Follower)
		{
			Follower->SetLeaderPoseComponent(nullptr);
			Follower->SetAnimInstanceClass(nullptr);
			Follower->SetEnableAnimation(false);
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

float AUEPlayerCharacter::GetHHVActionDuration(const FGameplayTag& StateTag, float FallbackDuration) const
{
	const UAnimSequence* Sequence = PlayerAnimationData
		? PlayerAnimationData->FindSequenceByTagForGender(StateTag, CurrentCustomizationGender)
		: nullptr;
	return FMath::Max(Sequence ? Sequence->GetPlayLength() : FallbackDuration, 0.01f);
}

void AUEPlayerCharacter::Roll()
{
	if (bIsRolling || ActionStateTag.IsValid())
	{
		return;
	}

	ActiveRollDirection = GetDesiredMovementDirection().GetSafeNormal2D();
	if (ActiveRollDirection.IsNearlyZero())
	{
		ActiveRollDirection = GetActorForwardVector().GetSafeNormal2D();
	}
	SetActorRotation(ActiveRollDirection.Rotation());

	CancelLanding();
	bIsRolling = true;
	RefreshCharacterState();
	float RollMontageDuration = 0.0f;
	if (UUEAnimInstance* PlayerAnimInstance = Cast<UUEAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		UAnimSequence* RollAnimation = nullptr;
		if (PlayerAnimationData)
		{
			const FUEPlayerGenderAnimationSet& AnimationSet =
				PlayerAnimationData->GetAnimationSetForGender(CurrentCustomizationGender);
			RollAnimation = AnimationSet.RollSequence
				? AnimationSet.RollSequence.Get()
				: PlayerAnimationData->RollSequence.Get();
		}
		RollMontageDuration = PlayerAnimInstance->PlayRollMontage(
			RollAnimation,
			RollAnimationPlayRate);
	}

	GetWorldTimerManager().ClearTimer(RollStateTimerHandle);
	GetWorldTimerManager().SetTimer(
		RollStateTimerHandle,
		this,
		&ThisClass::FinishRoll,
		RollMontageDuration > 0.0f
			? RollMontageDuration
			: GetHHVActionDuration(UEGameplayTags::State_Character_Roll, RollStateDuration),
		false);
}

void AUEPlayerCharacter::MakeAppearanceExternallyDriven()
{
	bAppearanceExternallyDriven = true;
}

void AUEPlayerCharacter::ApplyPendingHHVAppearance()
{
	// 로비 슬롯 프리뷰처럼 밖에서 외형을 지시받는 액터는 건드리지 않는다.
	// PendingHHVAppearance 는 "내 캐릭터" 의 착장이라, 슬롯마다 다른 캐릭터를
	// 그리는 자리에 입히면 방금 넣어준 서버 값을 덮어버린다.
	if (bAppearanceExternallyDriven)
	{
		return;
	}

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
	if (!Component || MorphSafeMaterialDirectory.Path.IsEmpty())
	{
		return;
	}

	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* OriginalMaterial = Component->GetMaterial(Index);
		const FString SafeName = MakeHHVMorphSafeMaterialName(OriginalMaterial);
		const FString SafePath = FString::Printf(
			TEXT("%s/%s.%s"),
			*MorphSafeMaterialDirectory.Path,
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
	const FLinearColor& /*EyeColor*/) const
{
	if (!Component || !EyeOption.Material)
	{
		return;
	}

	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		if (IsHHVEyeMaterialSlot(Component, Index))
		{
			// 커마 프리뷰와 동일하게 눈 프리셋의 원본 머티리얼을 그대로 사용한다.
			// 합성 텍스처나 임의 색 파라미터를 덮으면 흰자와 홍채가 검게 변한다.
			EnsurePlayerHHVSkeletalMaterialUsage(EyeOption.Material);
			Component->SetMaterial(Index, EyeOption.Material);
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
