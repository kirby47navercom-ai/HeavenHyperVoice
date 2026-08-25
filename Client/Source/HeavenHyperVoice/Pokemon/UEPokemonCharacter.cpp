// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPokemonCharacter.h"

#include "../AI/UEAIController.h"
#include "../Animation/UEPokemonAnimInstance.h"
#include "UEPokemonSpeciesData.h"
#include "Server/UEPokemonServerComponent.h"
#include "UEPokemonSpeciesCatalog.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// 서버 PokemonSpecies.h 의 종족 순서(1~20)에 맞춘 대표 색.
	//
	// 진짜 모델이 붙기 전까지 큐브를 칠하는 용도라 정확할 필요는 없고 구분만
	// 되면 된다. Client/Content/Pokemon/Asset 의 폴더가 들어오면 이 팔레트는
	// 쓰이지 않는다 (ApplyDebugAppearance 가 SkeletalMesh 있으면 바로 빠진다).
	//
	// 순서를 바꾸면 서버 id 와 어긋난다. PokemonSpecies.h 와 같이 고칠 것.
	const TArray<FLinearColor>& SpeciesPalette()
	{
		static const TArray<FLinearColor> Palette = {
			FLinearColor(0.45f, 0.35f, 0.20f),  //  1 귀뚤뚜기   갈색
			FLinearColor(0.25f, 0.20f, 0.30f),  //  2 기라티나   흑자
			FLinearColor(0.20f, 0.35f, 0.75f),  //  3 꼬링크     남청
			FLinearColor(0.35f, 0.65f, 0.85f),  //  4 꼬부기     하늘
			FLinearColor(0.70f, 0.90f, 0.95f),  //  5 꽁어름     얼음
			FLinearColor(0.30f, 0.55f, 0.95f),  //  6 디아루가   강청
			FLinearColor(0.90f, 0.85f, 0.90f),  //  7 랄토스     흰보라
			FLinearColor(0.35f, 0.70f, 0.35f),  //  8 모부기     초록
			FLinearColor(0.55f, 0.55f, 0.60f),  //  9 벼리짱     회색
			FLinearColor(0.95f, 0.50f, 0.20f),  // 10 불꽃숭이   주황
			FLinearColor(0.95f, 0.92f, 0.75f),  // 11 아르세우스 미백
			FLinearColor(0.75f, 0.60f, 0.35f),  // 12 이브이     갈황
			FLinearColor(0.30f, 0.75f, 0.45f),  // 13 이상해씨   연두
			FLinearColor(0.80f, 0.20f, 0.30f),  // 14 자망칼     적흑
			FLinearColor(0.40f, 0.30f, 0.25f),  // 15 터검니     암갈
			FLinearColor(0.95f, 0.35f, 0.15f),  // 16 파이리     붉은주황
			FLinearColor(0.95f, 0.95f, 0.45f),  // 17 파치리스   노랑
			FLinearColor(0.20f, 0.45f, 0.80f),  // 18 팽도리     짙은파랑
			FLinearColor(0.85f, 0.55f, 0.85f),  // 19 펄기아     분홍보라
			FLinearColor(1.00f, 0.85f, 0.10f),  // 20 피카츄     노랑
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

	// 카탈로그에 그 종족의 실제 모델이 있으면 그걸 쓴다. 아직 에셋이 없으면
	// 카탈로그가 비어 있고, 예전처럼 종족 색 큐브로 뜬다.
	if (PokemonSpeciesCatalog)
	{
		if (UUEPokemonSpeciesData* Data = PokemonSpeciesCatalog->Find(SpeciesNumber))
		{
			// SetPokemonSpeciesData 가 메시·캡슐·애니메이션까지 다 적용하고
			// ServerSpeciesId 를 데이터 쪽 이름으로 덮는다. 착색은 그 안에서
			// 알아서 빠진다 (SkeletalMesh 가 있으면 큐브를 안 건드린다).
			SetPokemonSpeciesData(Data);
			return;
		}
	}

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

	GetMesh()->SetRelativeLocation({0.0f,0.0f,-90.0f});

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
		// 플레이어 동행은 피카츄로 취급한다 (PokemonSpecies.h 의 20번).
		Color = ColorForSpecies(FName(TEXT("20")));
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

	// 서버가 선택한 필드 행동은 현재 종의 AnimInstance가 실제 시퀀스로 변환해 재생한다.
	// DataAsset에 없는 시퀀스는 AnimInstance에서 자동으로 건너뛴다.
	if (Snapshot.AnimationEvent == EUEPokemonAnimationEvent::FieldAnimationStarted)
	{
		if (UUEPokemonAnimInstance* PokemonAnimInstance = Cast<UUEPokemonAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			PokemonAnimInstance->PlayFieldAnimation(Snapshot.FieldAnimation, Snapshot.FieldAnimationLoopCount);
		}
	}
	else if (Snapshot.AnimationEvent == EUEPokemonAnimationEvent::AttackStarted)
	{
		// 서버가 승인한 공격 종류만 재생해 클라이언트 입력과 실제 포켓몬 상태가 어긋나지 않게 한다.
		if (UUEPokemonAnimInstance* PokemonAnimInstance = Cast<UUEPokemonAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			PokemonAnimInstance->PlayAttackAnimation(Snapshot.AttackAnimation, Snapshot.AttackAnimationLoopCount);
		}
	}

	BP_OnServerAnimationEvent(Snapshot.AnimationEvent, Snapshot);
}

void AUEPokemonCharacter::UpdateServerDrivenMovement(float DeltaSeconds)
{
	if (!bHasServerMoveTarget)
	{
		// 목표에 도달했다. 속도를 남겨두면 제자리에 서서 걷는 모션이 계속 돈다.
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
		return;
	}

	const FVector PreviousLocation = GetActorLocation();
	const FVector NewLocation = FMath::VInterpTo(PreviousLocation, TargetServerLocation, DeltaSeconds, ServerLocationInterpSpeed);
	const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetServerRotation, DeltaSeconds, ServerRotationInterpSpeed);

	SetActorLocation(NewLocation, false);
	SetActorRotation(NewRotation);

	// SetActorLocation 은 직접 옮기는 것이라 CharacterMovement 가 속도를 계산해 주지
	// 않는다. 여기서 채우지 않으면 GetVelocity() 가 0 이고, 애님 BP 의 GroundSpeed 가
	// 늘 0 이라 걸어다니는 동안에도 idle 만 나온다.
	//
	// 서버가 준 속도가 아니라 **이번 프레임에 실제로 움직인 양**에서 뽑는다. 보간
	// 때문에 화면에서 움직이는 속도는 서버 의도와 다른데, 애니메이션은 눈에 보이는
	// 움직임과 맞아야 한다.
	if (DeltaSeconds > SMALL_NUMBER)
	{
		GetCharacterMovement()->Velocity = (NewLocation - PreviousLocation) / DeltaSeconds;
	}

	if (FVector::DistSquared(NewLocation, TargetServerLocation) <= 1.0f)
	{
		SetActorLocation(TargetServerLocation, false);
		bHasServerMoveTarget = false;
	}
}
