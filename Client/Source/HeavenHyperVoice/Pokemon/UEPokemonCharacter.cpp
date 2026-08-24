// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPokemonCharacter.h"

#include "../AI/UEAIController.h"
#include "UEPokemonSpeciesData.h"
#include "Server/UEPokemonServerComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// 서버 PokemonSpecies.h 의 종족 순서(1~10)에 맞춘 대표 색. 실제 모델이
	// 없을 때 큐브를 칠하는 용도라 정확할 필요는 없고 구분만 되면 된다.
	const TArray<FLinearColor>& SpeciesPalette()
	{
		static const TArray<FLinearColor> Palette = {
			FLinearColor(1.00f, 0.85f, 0.10f),  // 1 피카츄   노랑
			FLinearColor(0.20f, 0.70f, 0.30f),  // 2 이상해꽃 초록
			FLinearColor(0.95f, 0.45f, 0.15f),  // 3 리자몽   주황
			FLinearColor(0.20f, 0.50f, 0.90f),  // 4 거북왕   파랑
			FLinearColor(0.55f, 0.55f, 0.60f),  // 5 딱구리   회색
			FLinearColor(0.80f, 0.65f, 0.30f),  // 6 후딘     황갈
			FLinearColor(0.15f, 0.25f, 0.45f),  // 7 잠만보   남색
			FLinearColor(0.55f, 0.30f, 0.70f),  // 8 포푸니라 보라
			FLinearColor(0.30f, 0.55f, 0.85f),  // 9 루카리오 청색
			FLinearColor(0.60f, 0.80f, 0.35f),  // 10 귀뚤톡크 연두
		};
		return Palette;
	}

	// 종족 id 로 팔레트 색을 고른다. id 가 "1".."10" 이면 그 자리, 아니면
	// 이름 해시로 아무 색이나 안정적으로 배정한다 (같은 종족은 늘 같은 색).
	FLinearColor ColorForSpecies(FName SpeciesId)
	{
		const TArray<FLinearColor>& Palette = SpeciesPalette();
		const FString Name = SpeciesId.ToString();
		if (Name.IsNumeric())
		{
			const int32 Number = FCString::Atoi(*Name);
			if (Number >= 1 && Number <= Palette.Num())
			{
				return Palette[Number - 1];
			}
		}

		const uint32 Hash = GetTypeHash(SpeciesId);
		return Palette[Hash % static_cast<uint32>(Palette.Num())];
	}
}

AUEPokemonCharacter::AUEPokemonCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AUEAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	ConfigureServerDrivenMovement();

	ServerComponent = CreateDefaultSubobject<UUEPokemonServerComponent>(TEXT("ServerComponent"));
}

void AUEPokemonCharacter::BeginPlay()
{
	Super::BeginPlay();

	ConfigureServerDrivenMovement();
	ApplyPokemonSpeciesData();
	TargetServerLocation = GetActorLocation();
	TargetServerRotation = GetActorRotation();
}

void AUEPokemonCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateServerDrivenMovement(DeltaSeconds);
}

void AUEPokemonCharacter::ApplyServerMoveSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot)
{
	ServerPokemonId = Snapshot.PokemonId;
	PokemonInstanceId = Snapshot.PokemonInstanceId;
	ServerSpeciesId = Snapshot.SpeciesId;
	SetRenderType(Snapshot.RenderType);
	ApplyServerStats(Snapshot.CurrentHP, Snapshot.MaxHP);
	ApplyServerAnimationSnapshot(Snapshot);
	ApplyServerMoveTarget(Snapshot.Location, Snapshot.Velocity, Snapshot.Rotation, Snapshot.bTeleported);
}

void AUEPokemonCharacter::SetPokemonSpeciesData(UUEPokemonSpeciesData* NewSpeciesData)
{
	if (PokemonSpeciesData == NewSpeciesData)
	{
		return;
	}

	PokemonSpeciesData = NewSpeciesData;
	ApplyPokemonSpeciesData();
}

void AUEPokemonCharacter::SetWildSpecies(int32 SpeciesNumber)
{
	// 종족 번호를 이름으로 박아 착색이 팔레트를 그대로 쓰게 한다.
	ServerSpeciesId = FName(*FString::FromInt(SpeciesNumber));
	RenderType = EUEPokemonRenderType::Wild;
	ApplyDebugAppearance();
}

FName AUEPokemonCharacter::GetPokemonSpeciesId() const
{
	return PokemonSpeciesData && !PokemonSpeciesData->SpeciesId.IsNone() ? PokemonSpeciesData->SpeciesId : ServerSpeciesId;
}

void AUEPokemonCharacter::SetRenderType(EUEPokemonRenderType NewRenderType)
{
	RenderType = NewRenderType;
}

void AUEPokemonCharacter::ConfigureServerDrivenMovement()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->GravityScale = 0.0f;
		MovementComponent->SetMovementMode(MOVE_None);
		MovementComponent->SetComponentTickEnabled(false);
	}
}

void AUEPokemonCharacter::ApplyPokemonSpeciesData()
{
	if (!PokemonSpeciesData)
	{
		return;
	}

	if (UCapsuleComponent* LocalCapsuleComponent = GetCapsuleComponent())
	{
		LocalCapsuleComponent->SetCapsuleSize(PokemonSpeciesData->CapsuleRadius, PokemonSpeciesData->CapsuleHalfHeight, true);
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (PokemonSpeciesData->SkeletalMesh)
		{
			MeshComponent->SetSkeletalMesh(PokemonSpeciesData->SkeletalMesh);
		}

		if (PokemonSpeciesData->AnimInstanceClass)
		{
			MeshComponent->SetAnimInstanceClass(PokemonSpeciesData->AnimInstanceClass);
		}

		MeshComponent->SetRelativeTransform(PokemonSpeciesData->MeshRelativeTransform);
	}

	ConfiguredMoveSpeed = PokemonSpeciesData->MoveSpeed;
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = ConfiguredMoveSpeed;
		MovementComponent->MaxStepHeight = PokemonSpeciesData->MaxStepHeight;
		MovementComponent->SetWalkableFloorAngle(PokemonSpeciesData->WalkableFloorAngleDegrees);
		ConfigureServerDrivenMovement();
	}

	MaxHP = FMath::Max(PokemonSpeciesData->MaxHP, 1.0f);
	CurrentHP = MaxHP;
	ServerSpeciesId = PokemonSpeciesData->SpeciesId;

	ApplyDebugAppearance();
}

void AUEPokemonCharacter::ApplyDebugAppearance()
{
	// 진짜 모델이 붙어 있으면 큐브를 건드리지 않는다.
	if (PokemonSpeciesData && PokemonSpeciesData->SkeletalMesh)
	{
		return;
	}

	// 한 번만 칠한다. 매번 CreateDynamicMaterialInstance 를 부르면 큐브가
	// 프레임마다 새 머티리얼로 바뀌며 깜빡인다.
	if (bDebugAppearanceApplied)
	{
		return;
	}
	bDebugAppearanceApplied = true;

	// 큐브는 BP_Pokemon 이 들고 있는 StaticMeshComponent 다. 런타임에 찾는다.
	UStaticMeshComponent* CubeComponent = FindComponentByClass<UStaticMeshComponent>();
	if (!CubeComponent)
	{
		return;
	}

	// DebugColor 의 알파가 0 이면 미설정 → 종족 팔레트. 아니면 데이터 값 우선.
	const FName Species = GetPokemonSpeciesId();
	FLinearColor Color;
	if (Species.IsNone() && RenderType == EUEPokemonRenderType::Own)
	{
		// 플레이어 동행은 피카츄로 취급한다 (팔레트 1번, 노랑).
		Color = ColorForSpecies(FName(TEXT("1")));
	}
	else
	{
		Color = ColorForSpecies(Species);
	}
	if (PokemonSpeciesData && PokemonSpeciesData->DebugColor.A > 0.f)
	{
		Color = PokemonSpeciesData->DebugColor;
	}

	// BasicShapeMaterial 에는 색 파라미터가 없어 이대로는 회색 큐브다. 색이
	// 실제로 나오게 하려면 Color(VectorParameter) 하나 있는 머티리얼을 큐브에
	// 물리면 된다 — 아래 관례 파라미터명들을 그대로 쓰면 코드 수정이 필요 없다.
	if (UMaterialInstanceDynamic* Dynamic = CubeComponent->CreateDynamicMaterialInstance(0))
	{
		Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
		Dynamic->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Dynamic->SetVectorParameterValue(TEXT("Tint"), Color);
	}
}

void AUEPokemonCharacter::ApplyServerStats(float ServerCurrentHP, float ServerMaxHP)
{
	if (ServerMaxHP <= 0.0f)
	{
		return;
	}

	MaxHP = FMath::Max(ServerMaxHP, 1.0f);
	CurrentHP = FMath::Clamp(ServerCurrentHP, 0.0f, MaxHP);
}

void AUEPokemonCharacter::ApplyServerMoveTarget(const FVector& ServerLocation, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bTeleported)
{
	TargetServerLocation = ServerLocation;
	TargetServerVelocity = ServerVelocity;
	TargetServerRotation = ServerRotation;
	bHasServerMoveTarget = true;

	const float DistanceToServer = FVector::Dist(GetActorLocation(), ServerLocation);
	if (bTeleported || DistanceToServer >= ServerHardSnapDistance)
	{
		SetActorLocation(ServerLocation, false, nullptr, ETeleportType::TeleportPhysics);
		SetActorRotation(ServerRotation, ETeleportType::TeleportPhysics);
		GetCharacterMovement()->Velocity = ServerVelocity;
		bHasServerMoveTarget = false;
	}
}

void AUEPokemonCharacter::ApplyServerAnimationSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot)
{
	ServerAnimationState = Snapshot.AnimationState;

	if (Snapshot.AnimationEvent == EUEPokemonAnimationEvent::None)
	{
		return;
	}

	LastServerAnimationEvent = Snapshot.AnimationEvent;
	LastServerAnimationEventTimeSeconds = Snapshot.ServerTimeSeconds;
	LastServerAnimationEventDurationSeconds = Snapshot.EventDurationSeconds;
	BP_OnServerAnimationEvent(Snapshot.AnimationEvent, Snapshot);
}

void AUEPokemonCharacter::UpdateServerDrivenMovement(float DeltaSeconds)
{
	if (!bHasServerMoveTarget)
	{
		return;
	}

	const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), TargetServerLocation, DeltaSeconds, ServerLocationInterpSpeed);
	const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetServerRotation, DeltaSeconds, ServerRotationInterpSpeed);

	SetActorLocation(NewLocation, false);
	SetActorRotation(NewRotation);
	GetCharacterMovement()->Velocity = TargetServerVelocity;

	if (FVector::DistSquared(NewLocation, TargetServerLocation) <= 1.0f)
	{
		SetActorLocation(TargetServerLocation, false);
		bHasServerMoveTarget = false;
	}
}
