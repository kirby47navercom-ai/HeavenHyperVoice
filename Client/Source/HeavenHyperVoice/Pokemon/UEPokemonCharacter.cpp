// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPokemonCharacter.h"

#include "../AbilitySystem/UEAbilitySystemComponent.h"
#include "../Animation/UEPokemonAnimInstance.h"
#include "../UI/HUD/UEHealthBarWidget.h"
#include "AbilitySystem/UEPokemonAttributeSet.h"
#include "UEPokemonSpeciesData.h"
#include "UEPokemonSpeciesCatalog.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

AUEPokemonCharacter::AUEPokemonCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = nullptr;
	AutoPossessAI = EAutoPossessAI::Disabled;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	ConfigureServerDrivenMovement();

	// 각 포켓몬이 자신의 ASC와 AttributeSet을 직접 소유한다.
	// 별도 PlayerState가 없는 야생 포켓몬도 같은 방식으로 GAS를 사용할 수 있다.
	AbilitySystemComponent = CreateDefaultSubobject<UUEAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AttributeSet = CreateDefaultSubobject<UUEPokemonAttributeSet>(TEXT("PokemonAttributeSet"));

	HealthBarWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidgetComponent"));
	HealthBarWidgetComponent->SetupAttachment(GetRootComponent());
	HealthBarWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidgetComponent->SetAbsolute(false, true, false);
	HealthBarWidgetComponent->SetDrawSize(FVector2D(220.0f, 52.0f));
	HealthBarWidgetComponent->SetPivot(FVector2D(0.5f, 1.0f));
	HealthBarWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 140.0f));
	HealthBarWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AUEPokemonCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();
	if (HealthBarWidgetComponent)
	{
		// 체력 UI만 포켓몬의 회전을 상속하지 않고 로컬 플레이어 화면을 정면으로 본다.
		HealthBarWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
		HealthBarWidgetComponent->SetAbsolute(false, true, false);
		if (HealthBarWidgetClass)
		{
			HealthBarWidgetComponent->SetWidgetClass(HealthBarWidgetClass);
			HealthBarWidgetComponent->InitWidget();
		}
	}
	ConfigureServerDrivenMovement();
	ApplyPokemonSpeciesData();
	RefreshHealthBarPosition();
	RefreshHealthBarWidget();
	RefreshWildCryTimer();
}

void AUEPokemonCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 시야 밖으로 사라진 야생 포켓몬의 예약 울음이 뒤늦게 실행되지 않게 정리한다.
	GetWorldTimerManager().ClearTimer(WildCryTimerHandle);
	if (EndPlayReason == EEndPlayReason::Destroyed && !bDespawnAudioPlayed)
	{
		bDespawnAudioPlayed = true;
		PlayPokemonSoundEffect(EUEPokemonSoundEffect::Despawn);
	}
	Super::EndPlay(EndPlayReason);
}

void AUEPokemonCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateServerDrivenMovement(DeltaSeconds);
}

void AUEPokemonCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	PlayPokemonSoundEffect(EUEPokemonSoundEffect::Jump);
}

void AUEPokemonCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	PlayPokemonSoundEffect(EUEPokemonSoundEffect::Landing);
}

UAbilitySystemComponent* AUEPokemonCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AUEPokemonCharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent || !AttributeSet || bAbilitySystemInitialized)
	{
		return;
	}

	// 포켓몬 액터 하나가 ASC의 소유자와 실제 전투 아바타를 모두 담당한다.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UUEPokemonAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AUEPokemonCharacter::HandleHealthChanged);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UUEPokemonAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &AUEPokemonCharacter::HandleMaxHealthChanged);
	bAbilitySystemInitialized = true;
}

void AUEPokemonCharacter::ApplyServerMoveSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot)
{
	// 같은 서버 틱에서 받은 식별자, 전투 수치, 애니메이션과 이동 목표를 함께 적용한다.
	ServerEntityId = static_cast<int64>(FMath::Max(Snapshot.PokemonId, 0));
	ServerPokemonId = Snapshot.PokemonId;
	PokemonInstanceId = Snapshot.PokemonInstanceId;
	ServerSpeciesId = Snapshot.SpeciesId;
	SetRenderType(Snapshot.RenderType);
	ApplyServerStats(Snapshot.CurrentHP, Snapshot.MaxHP);
	ApplyServerAnimationSnapshot(Snapshot);
	ApplyServerMoveTarget(Snapshot.Location, Snapshot.Velocity, Snapshot.Rotation, Snapshot.bTeleported, Snapshot.ServerTimeSeconds);
}

void AUEPokemonCharacter::InitializeServerEntity(
	int64 NewServerEntityId,
	int32 SpeciesNumber,
	EUEPokemonRenderType NewRenderType)
{
	ServerEntityId = NewServerEntityId;
	ServerPokemonId = NewServerEntityId >= 0 && NewServerEntityId <= static_cast<int64>(MAX_int32)
		? static_cast<int32>(NewServerEntityId)
		: 0;
	SetRenderType(NewRenderType);

	if (SpeciesNumber > 0)
	{
		SetWildSpecies(SpeciesNumber);
		SetRenderType(NewRenderType);
	}

	if (PokemonSpeciesData && !bSpawnAudioPlayed)
	{
		bSpawnAudioPlayed = true;
		PlayPokemonSoundEffect(EUEPokemonSoundEffect::Spawn);
		PlaySummonCry();
	}
}

void AUEPokemonCharacter::SetPokemonSpeciesData(UUEPokemonSpeciesData* NewSpeciesData)
{
	if (PokemonSpeciesData == NewSpeciesData)
	{
		return;
	}

	PokemonSpeciesData = NewSpeciesData;
	ApplyPokemonSpeciesData();
	RefreshWildCryTimer();
}

// SpeciesNumber 는 도감번호다 (field.fbs EntityState.species).
void AUEPokemonCharacter::SetWildSpecies(int32 SpeciesNumber)
{
	// 서버 종족 번호는 카탈로그 조회와 디버그 식별에만 사용한다.
	ServerSpeciesId = FName(*FString::FromInt(SpeciesNumber));
	// 직접 값을 대입하지 않고 Setter를 통과시켜, 이미 종족 데이터가 있는 재사용 액터도 야생 울음 타이머를 다시 켠다.
	SetRenderType(EUEPokemonRenderType::Wild);

	// 카탈로그에 그 종족의 실제 모델이 있으면 그걸 쓴다. 아직 에셋이 없으면
	// 카탈로그가 비어 있고, 예전처럼 종족 색 큐브로 뜬다.
	if (PokemonSpeciesCatalog)
	{
		// 배열 위치가 아니라 도감번호로 찾는다. 카탈로그에 종족을 끼워 넣어도
		// 야생 포켓몬이 다른 종족으로 바뀌지 않는다.
		if (UUEPokemonSpeciesData* Data = PokemonSpeciesCatalog->FindByDex(SpeciesNumber))
		{
			// SetPokemonSpeciesData 가 메시·애니메이션까지 다 적용하고
			// ServerSpeciesId 를 데이터 쪽 이름으로 덮는다. 착색은 그 안에서
			// 알아서 빠진다 (SkeletalMesh 가 있으면 큐브를 안 건드린다).
			SetPokemonSpeciesData(Data);
			return;
		}
	}

	ApplyDebugAppearance();
	RefreshHealthBarPosition();
	RefreshHealthBarWidget();
	RefreshWildCryTimer();
}

FName AUEPokemonCharacter::GetPokemonSpeciesId() const
{
	return PokemonSpeciesData && !PokemonSpeciesData->SpeciesId.IsNone() ? PokemonSpeciesData->SpeciesId : ServerSpeciesId;
}

FText AUEPokemonCharacter::GetPokemonDisplayName() const
{
	if (PokemonSpeciesData && !PokemonSpeciesData->DisplayName.IsEmpty())
	{
		return PokemonSpeciesData->DisplayName;
	}

	const FName SpeciesId = GetPokemonSpeciesId();
	return SpeciesId.IsNone() ? FText::GetEmpty() : FText::FromName(SpeciesId);
}

void AUEPokemonCharacter::SetRenderType(EUEPokemonRenderType NewRenderType)
{
	RenderType = NewRenderType;
	// 같은 포켓몬 액터가 야생과 동행 표현 사이를 오갈 때 타이머도 즉시 켜거나 끈다.
	RefreshWildCryTimer();
}

void AUEPokemonCharacter::PlaySummonCry()
{
	if (!PokemonSpeciesData)
	{
		return;
	}

	// 소환 전용 후보가 있으면 매번 하나를 새로 고르고, 미설정 종은 대표 울음으로 안전하게 대체한다.
	PlayCrySound(SelectRandomSound(PokemonSpeciesData->SummonCries, PokemonSpeciesData->SummonCry.Get()));
}

void AUEPokemonCharacter::PlayFaintCry()
{
	if (!PokemonSpeciesData)
	{
		return;
	}

	// 기절 전용 후보만 사용한다. 전용 소리가 없는 종이 갑자기 일반 소리를 내지 않도록 소환 울음은 대체재로 쓰지 않는다.
	PlayCrySound(SelectRandomSound(PokemonSpeciesData->FaintCries, PokemonSpeciesData->FaintCry.Get()));
}

void AUEPokemonCharacter::PlayRandomWildCry()
{
	if (RenderType != EUEPokemonRenderType::Wild || !PokemonSpeciesData)
	{
		return;
	}

	// 야생 후보에는 평온·기쁨·환경·특수음성만 넣는다. 전투·분노·슬픔 소리는 섞지 않는다.
	PlayCrySound(SelectRandomSound(PokemonSpeciesData->WildCries, PokemonSpeciesData->SummonCry.Get()));
}

void AUEPokemonCharacter::PlayRandomHappyCry()
{
	PlayCrySound(PokemonSpeciesData ? SelectRandomSound(PokemonSpeciesData->HappyCries) : nullptr);
}

void AUEPokemonCharacter::PlayRandomAngryCry()
{
	PlayCrySound(PokemonSpeciesData ? SelectRandomSound(PokemonSpeciesData->AngryCries) : nullptr);
}

void AUEPokemonCharacter::PlayRandomSadCry()
{
	PlayCrySound(PokemonSpeciesData ? SelectRandomSound(PokemonSpeciesData->SadCries) : nullptr);
}

void AUEPokemonCharacter::PlayRandomPhysicalAttackCry()
{
	PlayCrySound(PokemonSpeciesData ? SelectRandomSound(PokemonSpeciesData->PhysicalAttackCries) : nullptr);
}

void AUEPokemonCharacter::PlayRandomSpecialAttackCry()
{
	PlayCrySound(PokemonSpeciesData ? SelectRandomSound(PokemonSpeciesData->SpecialAttackCries) : nullptr);
}

void AUEPokemonCharacter::PlayRandomSpecialCry()
{
	PlayCrySound(PokemonSpeciesData ? SelectRandomSound(PokemonSpeciesData->SpecialCries) : nullptr);
}

void AUEPokemonCharacter::PlayRandomAmbientCry()
{
	PlayCrySound(PokemonSpeciesData ? SelectRandomSound(PokemonSpeciesData->AmbientCries) : nullptr);
}

void AUEPokemonCharacter::PlayPokemonSoundEffect(EUEPokemonSoundEffect Effect)
{
	if (!PokemonSpeciesData)
	{
		return;
	}

	const TArray<TObjectPtr<USoundBase>>* Candidates = nullptr;
	switch (Effect)
	{
	case EUEPokemonSoundEffect::Footstep: Candidates = &PokemonSpeciesData->FootstepSounds; break;
	case EUEPokemonSoundEffect::Jump: Candidates = &PokemonSpeciesData->JumpSounds; break;
	case EUEPokemonSoundEffect::Landing: Candidates = &PokemonSpeciesData->LandingSounds; break;
	case EUEPokemonSoundEffect::Swim: Candidates = &PokemonSpeciesData->SwimSounds; break;
	case EUEPokemonSoundEffect::SpecialMovement: Candidates = &PokemonSpeciesData->SpecialMovementSounds; break;
	case EUEPokemonSoundEffect::Attack: Candidates = &PokemonSpeciesData->AttackSounds; break;
	case EUEPokemonSoundEffect::Hit: Candidates = &PokemonSpeciesData->HitSounds; break;
	case EUEPokemonSoundEffect::Down: Candidates = &PokemonSpeciesData->DownSounds; break;
	case EUEPokemonSoundEffect::Faint: Candidates = &PokemonSpeciesData->FaintEffectSounds; break;
	case EUEPokemonSoundEffect::Eat: Candidates = &PokemonSpeciesData->EatSounds; break;
	case EUEPokemonSoundEffect::Stun: Candidates = &PokemonSpeciesData->StunSounds; break;
	case EUEPokemonSoundEffect::Sleep: Candidates = &PokemonSpeciesData->SleepSounds; break;
	case EUEPokemonSoundEffect::Spawn: Candidates = &PokemonSpeciesData->SpawnSounds; break;
	case EUEPokemonSoundEffect::Despawn: Candidates = &PokemonSpeciesData->DespawnSounds; break;
	default: break;
	}

	if (Candidates)
	{
		PlayEffectSound(SelectRandomSound(*Candidates));
	}
}

void AUEPokemonCharacter::RefreshWildCryTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(WildCryTimerHandle);
	if (RenderType != EUEPokemonRenderType::Wild || !PokemonSpeciesData || !PokemonSpeciesData->bEnableWildCries)
	{
		return;
	}

	bool bHasPlayableCry = PokemonSpeciesData->SummonCry != nullptr;
	for (const TObjectPtr<USoundBase>& Cry : PokemonSpeciesData->SummonCries)
	{
		bHasPlayableCry |= Cry != nullptr;
	}
	for (const TObjectPtr<USoundBase>& Cry : PokemonSpeciesData->WildCries)
	{
		bHasPlayableCry |= Cry != nullptr;
	}
	if (!bHasPlayableCry)
	{
		return;
	}

	// 매 개체가 서로 다른 시점에 울도록 반복 타이머 대신 다음 한 번의 대기 시간을 매번 새로 뽑는다.
	const float MinInterval = FMath::Max(PokemonSpeciesData->WildCryMinIntervalSeconds, 0.1f);
	const float MaxInterval = FMath::Max(PokemonSpeciesData->WildCryMaxIntervalSeconds, MinInterval);
	World->GetTimerManager().SetTimer(
		WildCryTimerHandle,
		this,
		&ThisClass::HandleWildCryTimer,
		FMath::FRandRange(MinInterval, MaxInterval),
		false);
}

void AUEPokemonCharacter::HandleWildCryTimer()
{
	PlayRandomWildCry();
	RefreshWildCryTimer();
}

USoundBase* AUEPokemonCharacter::SelectRandomSound(
	const TArray<TObjectPtr<USoundBase>>& Candidates,
	USoundBase* FallbackSound) const
{
	// DataAsset 편집 중 생긴 빈 칸은 후보에서 제외해 nullptr가 무작위로 선택되는 일을 막는다.
	TArray<USoundBase*> ValidSounds;
	ValidSounds.Reserve(Candidates.Num());
	for (const TObjectPtr<USoundBase>& Sound : Candidates)
	{
		if (Sound)
		{
			ValidSounds.Add(Sound.Get());
		}
	}

	return ValidSounds.IsEmpty()
		? FallbackSound
		: ValidSounds[FMath::RandRange(0, ValidSounds.Num() - 1)];
}

void AUEPokemonCharacter::PlayCrySound(USoundBase* CrySound) const
{
	if (!CrySound || !GetRootComponent())
	{
		return;
	}

	const float VolumeMultiplier = PokemonSpeciesData
		? FMath::Max(PokemonSpeciesData->CryVolumeMultiplier, 0.0f)
		: 1.0f;
	const float PitchMultiplier = PokemonSpeciesData
		? FMath::Max(PokemonSpeciesData->CryPitchMultiplier, 0.01f)
		: 1.0f;

	// 포켓몬의 현재 위치를 따라가는 3D 소리다. DataAsset의 공용 감쇠/동시 재생 에셋으로 거리감과 겹침을 통제한다.
	UGameplayStatics::SpawnSoundAttached(
		CrySound,
		GetRootComponent(),
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		true,
		VolumeMultiplier,
		PitchMultiplier,
		0.0f,
		PokemonSpeciesData ? PokemonSpeciesData->CryAttenuation.Get() : nullptr,
		PokemonSpeciesData ? PokemonSpeciesData->CryConcurrency.Get() : nullptr,
		true);
}

void AUEPokemonCharacter::PlayEffectSound(USoundBase* EffectSound) const
{
	if (!EffectSound || !PokemonSpeciesData)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		EffectSound,
		GetActorLocation(),
		FRotator::ZeroRotator,
		FMath::Max(PokemonSpeciesData->EffectVolumeMultiplier, 0.0f),
		FMath::Max(PokemonSpeciesData->EffectPitchMultiplier, 0.01f),
		0.0f,
		PokemonSpeciesData->EffectAttenuation
			? PokemonSpeciesData->EffectAttenuation.Get()
			: PokemonSpeciesData->CryAttenuation.Get(),
		PokemonSpeciesData->EffectConcurrency
			? PokemonSpeciesData->EffectConcurrency.Get()
			: PokemonSpeciesData->CryConcurrency.Get(),
		this);
}

void AUEPokemonCharacter::UpdateMovementSound(const FVector& PreviousLocation, const FVector& NewLocation)
{
	if (!PokemonSpeciesData)
	{
		return;
	}

	AccumulatedMovementSoundDistance += FVector::Dist2D(PreviousLocation, NewLocation);
	const float Interval = FMath::Max(PokemonSpeciesData->MovementSoundIntervalDistance, 1.0f);
	if (AccumulatedMovementSoundDistance < Interval)
	{
		return;
	}
	AccumulatedMovementSoundDistance = FMath::Fmod(AccumulatedMovementSoundDistance, Interval);

	const APhysicsVolume* PhysicsVolume = GetPhysicsVolume();
	if (PhysicsVolume && PhysicsVolume->bWaterVolume && !PokemonSpeciesData->SwimSounds.IsEmpty())
	{
		PlayPokemonSoundEffect(EUEPokemonSoundEffect::Swim);
	}
	else if (!PokemonSpeciesData->FootstepSounds.IsEmpty())
	{
		PlayPokemonSoundEffect(EUEPokemonSoundEffect::Footstep);
	}
	else
	{
		PlayPokemonSoundEffect(EUEPokemonSoundEffect::SpecialMovement);
	}
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

	const float SpeciesMaxHealth = FMath::Max(PokemonSpeciesData->MaxHP, 1.0f);
	InitializePokemonAttributes(
		SpeciesMaxHealth,
		SpeciesMaxHealth,
		PokemonSpeciesData->BaseAttackPower,
		PokemonSpeciesData->BaseDefense);

	// 종족 DataAsset에 지정한 GameplayAbility를 생성 시점에 자동으로 부여한다.
	if (bAbilitySystemInitialized && AbilitySystemComponent)
	{
		AbilitySystemComponent->SetCharacterAbilities(PokemonSpeciesData->StartupAbilities);
	}
	ServerSpeciesId = PokemonSpeciesData->SpeciesId;

	ApplyDebugAppearance();
	RefreshHealthBarPosition();
	RefreshHealthBarWidget();
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

	// 색은 종족 데이터 에셋이 소유한다. 값이 없으면 원본 머티리얼을 그대로 둔다.
	if (!PokemonSpeciesData || PokemonSpeciesData->DebugColor.A <= 0.0f)
	{
		return;
	}
	const FLinearColor Color = PokemonSpeciesData->DebugColor;

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

	// 외부 서버 스냅샷도 GAS 속성에 넣어 UI와 피격 델리게이트가 같은 경로로 반응하게 한다.
	const float SafeMaxHealth = FMath::Max(ServerMaxHP, 1.0f);
	InitializePokemonAttributes(
		FMath::Clamp(ServerCurrentHP, 0.0f, SafeMaxHealth),
		SafeMaxHealth,
		GetAttackPower(),
		GetDefense());
}

void AUEPokemonCharacter::InitializePokemonAttributes(
	float NewCurrentHealth,
	float NewMaxHealth,
	float NewAttackPower,
	float NewDefense)
{
	const float SafeMaxHealth = FMath::Max(NewMaxHealth, 1.0f);
	const float SafeCurrentHealth = FMath::Clamp(NewCurrentHealth, 0.0f, SafeMaxHealth);

	if (!AbilitySystemComponent || !AttributeSet)
	{
		// 생성 초기처럼 ASC가 아직 준비되지 않은 경우 기존 서버 호환용 값은 유지한다.
		MaxHP = SafeMaxHealth;
		CurrentHP = SafeCurrentHealth;
		return;
	}

	// SetNumericAttributeBase를 사용해야 값 변경 델리게이트와 GAS 집계기가 함께 갱신된다.
	AbilitySystemComponent->SetNumericAttributeBase(UUEPokemonAttributeSet::GetMaxHealthAttribute(), SafeMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UUEPokemonAttributeSet::GetHealthAttribute(), SafeCurrentHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UUEPokemonAttributeSet::GetAttackPowerAttribute(), FMath::Max(NewAttackPower, 0.0f));
	AbilitySystemComponent->SetNumericAttributeBase(UUEPokemonAttributeSet::GetDefenseAttribute(), FMath::Max(NewDefense, 0.0f));

	// 외부 필드 서버 코드가 기존 Getter를 그대로 사용할 수 있도록 호환용 값도 맞춘다.
	MaxHP = AttributeSet->GetMaxHealth();
	CurrentHP = AttributeSet->GetHealth();
}

float AUEPokemonCharacter::GetCurrentHP() const
{
	return AttributeSet ? AttributeSet->GetHealth() : CurrentHP;
}

float AUEPokemonCharacter::GetMaxHP() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : MaxHP;
}

float AUEPokemonCharacter::GetAttackPower() const
{
	return AttributeSet ? AttributeSet->GetAttackPower() : 0.0f;
}

float AUEPokemonCharacter::GetDefense() const
{
	return AttributeSet ? AttributeSet->GetDefense() : 0.0f;
}

float AUEPokemonCharacter::SetPokemonHealth(float NewHealth)
{
	const float OldHealth = GetCurrentHP();
	const float ClampedHealth = FMath::Clamp(NewHealth, 0.0f, GetMaxHP());

	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->SetNumericAttributeBase(UUEPokemonAttributeSet::GetHealthAttribute(), ClampedHealth);
	}
	else
	{
		CurrentHP = ClampedHealth;
		OnPokemonHealthChanged.Broadcast(this, OldHealth, CurrentHP, MaxHP);
		RefreshHealthBarWidget();
	}

	return GetCurrentHP();
}

float AUEPokemonCharacter::ApplyPokemonDamage(float DamageAmount)
{
	const float OldHealth = GetCurrentHP();
	SetPokemonHealth(OldHealth - FMath::Max(DamageAmount, 0.0f));
	const float AppliedDamage = OldHealth - GetCurrentHP();
	if (AppliedDamage > 0.0f && GetCurrentHP() > 0.0f)
	{
		PlayPokemonSoundEffect(EUEPokemonSoundEffect::Hit);
	}
	return AppliedDamage;
}

float AUEPokemonCharacter::RestorePokemonHealth(float HealAmount)
{
	const float OldHealth = GetCurrentHP();
	SetPokemonHealth(OldHealth + FMath::Max(HealAmount, 0.0f));
	return GetCurrentHP() - OldHealth;
}

bool AUEPokemonCharacter::ActivatePokemonAbilityByTag(FGameplayTag AbilityTag)
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->ActivateAbility(AbilityTag))
	{
		return false;
	}

	OnPokemonAbilityActivated.Broadcast(this, AbilityTag);
	return true;
}

void AUEPokemonCharacter::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	CurrentHP = FMath::Clamp(ChangeData.NewValue, 0.0f, GetMaxHP());
	MaxHP = AttributeSet ? AttributeSet->GetMaxHealth() : MaxHP;
	OnPokemonHealthChanged.Broadcast(this, ChangeData.OldValue, CurrentHP, MaxHP);
	RefreshHealthBarWidget();

	// 여러 GameplayEffect가 같은 프레임에 체력을 0으로 만들어도 기절 이벤트는 한 번만 보낸다.
	if (CurrentHP <= 0.0f)
	{
		if (!bFaintDelegateBroadcast)
		{
			bFaintDelegateBroadcast = true;
			// 일반 울음과 기절 울음을 구분한다. 전용 파일이 없는 종은 조용히 넘어간다.
			PlayFaintCry();
			PlayPokemonSoundEffect(EUEPokemonSoundEffect::Faint);
			OnPokemonFainted.Broadcast(this);
		}
	}
	else
	{
		// 회복이나 재소환으로 체력이 생기면 다음 기절을 다시 알릴 수 있게 푼다.
		bFaintDelegateBroadcast = false;
	}
}

void AUEPokemonCharacter::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	MaxHP = FMath::Max(ChangeData.NewValue, 1.0f);

	if (GetCurrentHP() > MaxHP)
	{
		SetPokemonHealth(MaxHP);
		return;
	}

	// 최대 체력만 변한 경우에도 체력바가 비율을 다시 계산할 수 있도록 같은 델리게이트를 보낸다.
	OnPokemonHealthChanged.Broadcast(this, GetCurrentHP(), GetCurrentHP(), MaxHP);
	RefreshHealthBarWidget();
}

void AUEPokemonCharacter::RefreshHealthBarWidget()
{
	if (!HealthBarWidgetComponent)
	{
		return;
	}

	if (UUEHealthBarWidget* HealthBarWidget = Cast<UUEHealthBarWidget>(HealthBarWidgetComponent->GetUserWidgetObject()))
	{
		HealthBarWidget->SetDisplayName(GetPokemonDisplayName());
		HealthBarWidget->SetHealth(GetCurrentHP(), GetMaxHP());
	}
}

void AUEPokemonCharacter::RefreshHealthBarPosition()
{
	if (!HealthBarWidgetComponent)
	{
		return;
	}

	UPrimitiveComponent* VisualComponent = nullptr;
	if (USkeletalMeshComponent* MeshComponent = GetMesh(); MeshComponent && MeshComponent->GetSkeletalMeshAsset())
	{
		VisualComponent = MeshComponent;
	}
	else
	{
		VisualComponent = FindComponentByClass<UStaticMeshComponent>();
	}

	if (!VisualComponent)
	{
		return;
	}

	const FBoxSphereBounds Bounds = VisualComponent->Bounds;
	const FVector WorldTop = Bounds.Origin + FVector::UpVector * Bounds.BoxExtent.Z;
	const FVector LocalTop = GetActorTransform().InverseTransformPosition(WorldTop);
	HealthBarWidgetComponent->SetRelativeLocation(FVector(LocalTop.X, LocalTop.Y, LocalTop.Z + HealthBarHeadOffset));
}

void AUEPokemonCharacter::ApplyServerMoveTarget(const FVector& ServerLocation, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bTeleported, double ServerTimeSeconds)
{
	const bool bReset = bTeleported || ServerMoveBuffer.IsEmpty() ||
		FVector::Dist(GetActorLocation(), ServerLocation) >= ServerHardSnapDistance;
	const double SampleTime = ServerTimeSeconds > 0.0 ? ServerTimeSeconds : FPlatformTime::Seconds();
	const FUEServerMoveSample Sample{SampleTime, ServerLocation, ServerVelocity, ServerRotation.Quaternion()};
	if (!ServerMoveBuffer.Add(Sample, bReset, ServerSnapshotIntervalSeconds * 2.0))
	{
		return;
	}
	if (bReset)
	{
		SetActorLocation(ServerLocation, false, nullptr, ETeleportType::TeleportPhysics);
		SetActorRotation(ServerRotation, ETeleportType::TeleportPhysics);
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
		AccumulatedMovementSoundDistance = 0.0f;
	}
}

void AUEPokemonCharacter::HandleServerAttackSignal(uint64 TargetEntityId, uint32 AttackSequence)
{
	if (AttackSequence == 0 || AttackSequence == LastServerAttackSequence)
	{
		return;
	}

	// 서버 시퀀스를 기억해 같은 공격 패킷이 다시 와도 소리와 모션을 중복 재생하지 않는다.
	LastServerAttackSequence = AttackSequence;
	LastServerAttackTargetId = TargetEntityId;

	PlayRandomPhysicalAttackCry();
	if (UUEPokemonAnimInstance* PokemonAnimInstance = Cast<UUEPokemonAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		PokemonAnimInstance->PlayAttackAnimation(EUEPokemonAttackAnimation::Attack01);
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
	else if (Snapshot.AnimationEvent == EUEPokemonAnimationEvent::SpawnStarted)
	{
		if (!bSpawnAudioPlayed)
		{
			bSpawnAudioPlayed = true;
			PlayPokemonSoundEffect(EUEPokemonSoundEffect::Spawn);
			PlaySummonCry();
		}
	}
	else if (Snapshot.AnimationEvent == EUEPokemonAnimationEvent::DespawnStarted)
	{
		if (!bDespawnAudioPlayed)
		{
			bDespawnAudioPlayed = true;
			PlayPokemonSoundEffect(EUEPokemonSoundEffect::Despawn);
		}
	}
	else if (Snapshot.AnimationEvent == EUEPokemonAnimationEvent::HitReact)
	{
		PlayPokemonSoundEffect(EUEPokemonSoundEffect::Hit);
	}
	else if (Snapshot.AnimationEvent == EUEPokemonAnimationEvent::Fainted)
	{
		PlayPokemonSoundEffect(EUEPokemonSoundEffect::Down);
	}

	BP_OnServerAnimationEvent(Snapshot.AnimationEvent, Snapshot);
}

void AUEPokemonCharacter::UpdateServerDrivenMovement(float DeltaSeconds)
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	FUEServerMoveSample Sample;
	if (!ServerMoveBuffer.Advance(DeltaSeconds, ServerSnapshotIntervalSeconds * 2.0, Sample))
	{
		if (MovementComponent)
		{
			MovementComponent->Velocity = FVector::ZeroVector;
		}
		return;
	}

	const FVector PreviousLocation = GetActorLocation();
	SetActorLocation(Sample.Location, false);
	SetActorRotation(Sample.Rotation);

	if (MovementComponent)
	{
		// 애니메이션에는 서버가 지시한 목표 속도가 아니라 화면에서 실제로 이동한 속도를 전달한다.
		// 보간 중 남은 이동과 정지 구간까지 같은 기준을 사용해야 발이 땅에서 덜 미끄러진다.
		const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, UE_SMALL_NUMBER);
		MovementComponent->Velocity = (GetActorLocation() - PreviousLocation) / SafeDeltaSeconds;
	}

	UpdateMovementSound(PreviousLocation, GetActorLocation());
}
