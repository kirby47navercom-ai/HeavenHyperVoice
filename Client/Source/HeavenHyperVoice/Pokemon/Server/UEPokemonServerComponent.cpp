// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPokemonServerComponent.h"

#include "../UEPokemonCharacter.h"
#include "../UEPokemonSpeciesData.h"

#include "Components/CapsuleComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Paths.h"

namespace
{
	int32 GNextPokemonServerId = 1;

	FVector ToLocalUnrealVector(const HHV::Map::Vec3& Vector)
	{
		return FVector(Vector.X, Vector.Y, Vector.Z);
	}

	bool TryFindNextPathTarget(const HHV::PokemonAI::Command& Command, const FVector& CurrentTarget, float AcceptanceRadius, FVector& OutNextTarget)
	{
		const std::vector<HHV::Map::Vec3>& PathPoints = Command.PathPoints;
		if (PathPoints.size() <= 1)
		{
			return false;
		}

		int32 ClosestTargetIndex = INDEX_NONE;
		float ClosestTargetDistanceSquared = BIG_NUMBER;
		for (int32 Index = 0; Index < static_cast<int32>(PathPoints.size()); ++Index)
		{
			const FVector PathPoint = ToLocalUnrealVector(PathPoints[static_cast<std::size_t>(Index)]);
			const float DistanceSquared = FVector::DistSquared(PathPoint, CurrentTarget);
			if (DistanceSquared < ClosestTargetDistanceSquared)
			{
				ClosestTargetDistanceSquared = DistanceSquared;
				ClosestTargetIndex = Index;
			}
		}

		if (ClosestTargetIndex == INDEX_NONE || ClosestTargetIndex >= static_cast<int32>(PathPoints.size()) - 1)
		{
			return false;
		}

		const float MinimumAdvanceDistanceSquared = FMath::Square(FMath::Max(AcceptanceRadius, 1.0f));
		for (int32 Index = ClosestTargetIndex + 1; Index < static_cast<int32>(PathPoints.size()); ++Index)
		{
			const FVector PathPoint = ToLocalUnrealVector(PathPoints[static_cast<std::size_t>(Index)]);
			if (FVector::DistSquared2D(PathPoint, CurrentTarget) > MinimumAdvanceDistanceSquared)
			{
				OutNextTarget = PathPoint;
				return true;
			}
		}

		return false;
	}

	float GetSequenceDuration(const UAnimSequence* Sequence, int32 LoopCount = 1)
	{
		return Sequence ? Sequence->GetPlayLength() * FMath::Max(LoopCount, 1) : 0.0f;
	}

	float GetFieldAnimationDuration(const UUEPokemonSpeciesData& SpeciesData, EUEPokemonFieldAnimation FieldAnimation, int32 LoopCount)
	{
		// 서버도 클라이언트 AnimInstance와 같은 순서로 길이를 더해 행동이 끝날 때까지 이동을 잠근다.
		switch (FieldAnimation)
		{
		case EUEPokemonFieldAnimation::Idle01:
			return GetSequenceDuration(SpeciesData.Idle01);
		case EUEPokemonFieldAnimation::Idle02:
			return GetSequenceDuration(SpeciesData.Idle02);
		case EUEPokemonFieldAnimation::TurnLeft90:
			return GetSequenceDuration(SpeciesData.TurnLeft90);
		case EUEPokemonFieldAnimation::TurnRight90:
			return GetSequenceDuration(SpeciesData.TurnRight90);
		case EUEPokemonFieldAnimation::Eat01:
			return GetSequenceDuration(SpeciesData.Eat01Start)
				+ GetSequenceDuration(SpeciesData.Eat01Loop, LoopCount)
				+ GetSequenceDuration(SpeciesData.Eat01End);
		case EUEPokemonFieldAnimation::Eat02:
			return GetSequenceDuration(SpeciesData.Eat02Start)
				+ GetSequenceDuration(SpeciesData.Eat02Loop, LoopCount)
				+ GetSequenceDuration(SpeciesData.Eat02End);
		case EUEPokemonFieldAnimation::Sleep:
			return GetSequenceDuration(SpeciesData.SleepStart)
				+ GetSequenceDuration(SpeciesData.SleepLoop, LoopCount)
				+ GetSequenceDuration(SpeciesData.SleepEnd);
		case EUEPokemonFieldAnimation::Rest:
			return GetSequenceDuration(SpeciesData.RestStart)
				+ GetSequenceDuration(SpeciesData.RestLoop, LoopCount)
				+ GetSequenceDuration(SpeciesData.RestEnd);
		case EUEPokemonFieldAnimation::Notice:
			return GetSequenceDuration(SpeciesData.Notice);
		case EUEPokemonFieldAnimation::Roar:
			return GetSequenceDuration(SpeciesData.Roar);
		case EUEPokemonFieldAnimation::Glad:
			return GetSequenceDuration(SpeciesData.Glad);
		case EUEPokemonFieldAnimation::Hate:
			return GetSequenceDuration(SpeciesData.Hate);
		case EUEPokemonFieldAnimation::Refresh:
			return GetSequenceDuration(SpeciesData.Refresh);
		case EUEPokemonFieldAnimation::StepOut:
			return GetSequenceDuration(SpeciesData.StepOutStart)
				+ GetSequenceDuration(SpeciesData.StepOut)
				+ GetSequenceDuration(SpeciesData.StepOutEnd);
		case EUEPokemonFieldAnimation::None:
		default:
			return 0.0f;
		}
	}

	float GetAttackAnimationDuration(const UUEPokemonSpeciesData& SpeciesData, EUEPokemonAttackAnimation AttackAnimation, int32 LoopCount)
	{
		// 서버가 계산한 이동 잠금 시간과 클라이언트가 재생하는 종별 공격 시퀀스 길이를 같게 맞춘다.
		switch (AttackAnimation)
		{
		case EUEPokemonAttackAnimation::Attack01:
			return GetSequenceDuration(SpeciesData.Attack01);
		case EUEPokemonAttackAnimation::Attack02:
			return GetSequenceDuration(SpeciesData.Attack02);
		case EUEPokemonAttackAnimation::RangeAttack01:
			return GetSequenceDuration(SpeciesData.RangeAttack01);
		case EUEPokemonAttackAnimation::RangeAttack02:
			return GetSequenceDuration(SpeciesData.RangeAttack02Start)
				+ GetSequenceDuration(SpeciesData.RangeAttack02Loop, LoopCount)
				+ GetSequenceDuration(SpeciesData.RangeAttack02End);
		case EUEPokemonAttackAnimation::None:
		default:
			return 0.0f;
		}
	}
}

UUEPokemonServerComponent::UUEPokemonServerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UUEPokemonServerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ServerPokemonId <= 0)
	{
		ServerPokemonId = GNextPokemonServerId++;
	}

	if (const AUEPokemonCharacter* PokemonCharacter = GetPokemonOwner())
	{
		ServerSimulatedLocation = PokemonCharacter->GetActorLocation();
		ServerSimulatedRotation = PokemonCharacter->GetActorRotation();
		if (ServerMaxHP <= 0.0f)
		{
			ServerMaxHP = PokemonCharacter->GetMaxHP();
			ServerCurrentHP = PokemonCharacter->GetCurrentHP();
		}
	}

	const int32 Seed = WanderRandomSeed != 0 ? WanderRandomSeed : ServerPokemonId * 7919 + 104729;
	WildBrain.SetRandomSeed(static_cast<std::uint32_t>(Seed));
	FieldAnimationRandomStream.Initialize(Seed ^ 0x45D9F3B);

	TryLoadServerMap();
}

void UUEPokemonServerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnableServer)
	{
		return;
	}

	const float ServerTickInterval = 1.0f / FMath::Max(ServerTickRate, 1.0f);
	ServerTickAccumulator += DeltaTime;
	if (ServerTickAccumulator < ServerTickInterval)
	{
		return;
	}

	const float SimulatedDeltaSeconds = ServerTickAccumulator;
	ServerTickAccumulator = 0.0f;
	RunServerSimulationTick(SimulatedDeltaSeconds);
}

void UUEPokemonServerComponent::SetFollowTargetActor(AActor* NewFollowTargetActor)
{
	FollowTargetActor = NewFollowTargetActor;
}

void UUEPokemonServerComponent::SetServerSimulationEnabled(bool bNewEnabled)
{
	bEnableServer = bNewEnabled;
	SetComponentTickEnabled(bEnableServer);
}

void UUEPokemonServerComponent::SetServerSimulationMode(EUEPokemonServerSimulationMode NewSimulationMode)
{
	if (SimulationMode == NewSimulationMode)
	{
		return;
	}

	SimulationMode = NewSimulationMode;
	ResetWildState();
}

void UUEPokemonServerComponent::InitializeServerRuntimePokemon(int32 RuntimePokemonId, int32 PokemonInstanceId, float CurrentHP, float MaxHP)
{
	if (RuntimePokemonId > 0)
	{
		ServerPokemonId = RuntimePokemonId;
		const int32 Seed = WanderRandomSeed != 0 ? WanderRandomSeed : ServerPokemonId * 7919 + 104729;
		WildBrain.SetRandomSeed(static_cast<std::uint32_t>(Seed));
		FieldAnimationRandomStream.Initialize(Seed ^ 0x45D9F3B);
		FieldAnimationBag.Reset();
	}

	ServerPokemonInstanceId = FMath::Max(PokemonInstanceId, 0);
	if (MaxHP > 0.0f)
	{
		ServerMaxHP = FMath::Max(MaxHP, 1.0f);
		ServerCurrentHP = FMath::Clamp(CurrentHP, 0.0f, ServerMaxHP);
		return;
	}

	if (const AUEPokemonCharacter* PokemonCharacter = GetPokemonOwner())
	{
		ServerMaxHP = PokemonCharacter->GetMaxHP();
		ServerCurrentHP = PokemonCharacter->GetCurrentHP();
	}
}

void UUEPokemonServerComponent::SendServerAnimationEvent(EUEPokemonAnimationEvent AnimationEvent, EUEPokemonAnimationState AnimationState, float EventDurationSeconds)
{
	AUEPokemonCharacter* PokemonCharacter = GetPokemonOwner();
	if (!PokemonCharacter)
	{
		return;
	}

	ServerSimulatedLocation = PokemonCharacter->GetActorLocation();
	ServerSimulatedRotation = PokemonCharacter->GetActorRotation();
	ServerSimulatedVelocity = FVector::ZeroVector;

	const float CurrentServerTimeSeconds = GetServerTimeSeconds();
	if (EventDurationSeconds > 0.0f)
	{
		bHasForcedAnimationState = true;
		ForcedAnimationState = AnimationState;
		ForcedAnimationStateEndServerTimeSeconds = CurrentServerTimeSeconds + EventDurationSeconds;
	}

	SendServerSnapshot(
		*PokemonCharacter,
		ServerSimulatedLocation,
		ServerSimulatedVelocity,
		ServerSimulatedRotation,
		false,
		AnimationState,
		AnimationEvent,
		EventDurationSeconds
	);
}

bool UUEPokemonServerComponent::SendServerAttackCommand(EUEPokemonAttackAnimation AttackAnimation, int32 LoopCount)
{
	AUEPokemonCharacter* PokemonCharacter = GetPokemonOwner();
	if (!PokemonCharacter || PokemonCharacter->GetRenderType() != EUEPokemonRenderType::Own)
	{
		// 현재 예시는 플레이어가 소유하고 필드에 꺼낸 포켓몬만 공격 명령을 받을 수 있다.
		return false;
	}

	const UUEPokemonSpeciesData* SpeciesData = PokemonCharacter->GetPokemonSpeciesData();
	const int32 SafeLoopCount = FMath::Clamp(LoopCount, 1, 10);
	const float AttackDuration = SpeciesData
		? GetAttackAnimationDuration(*SpeciesData, AttackAnimation, SafeLoopCount)
		: 0.0f;
	if (AttackDuration <= 0.0f)
	{
		// 종별 DataAsset에 선택한 슬롯의 공격 시퀀스가 없으면 명령을 승인하지 않는다.
		return false;
	}

	ServerSimulatedLocation = PokemonCharacter->GetActorLocation();
	ServerSimulatedRotation = PokemonCharacter->GetActorRotation();
	ServerSimulatedVelocity = FVector::ZeroVector;
	bIsFieldAnimationActive = false;
	bIsAttackAnimationActive = true;
	AttackAnimationEndServerTimeSeconds = GetServerTimeSeconds() + AttackDuration + 0.1f;
	bHasForcedAnimationState = true;
	ForcedAnimationState = EUEPokemonAnimationState::Attacking;
	ForcedAnimationStateEndServerTimeSeconds = AttackAnimationEndServerTimeSeconds;

	// 공격 종류를 스냅샷에 실어 AnimInstance가 현재 종의 실제 시퀀스를 고르게 한다.
	SendServerSnapshot(
		*PokemonCharacter,
		ServerSimulatedLocation,
		ServerSimulatedVelocity,
		ServerSimulatedRotation,
		false,
		EUEPokemonAnimationState::Attacking,
		EUEPokemonAnimationEvent::AttackStarted,
		AttackDuration,
		EUEPokemonFieldAnimation::None,
		1,
		AttackAnimation,
		SafeLoopCount
	);
	return true;
}

void UUEPokemonServerComponent::TryLoadServerMap()
{
	const FString MapFilePath = ResolveServerMapFilePath();
	if (MapFilePath.IsEmpty() || !FPaths::FileExists(MapFilePath))
	{
		bServerMapLoaded = false;
		return;
	}

	bServerMapLoaded = ServerMapRuntime.LoadFromFile(TCHAR_TO_UTF8(*MapFilePath));
	UE_LOG(
		LogTemp,
		Display,
		TEXT("PokemonServer: server map %s: %s"),
		bServerMapLoaded ? TEXT("loaded") : TEXT("failed"),
		*MapFilePath
	);
}

void UUEPokemonServerComponent::RunServerSimulationTick(float DeltaSeconds)
{
	AUEPokemonCharacter* PokemonCharacter = GetPokemonOwner();
	if (!PokemonCharacter)
	{
		return;
	}

	// 공격 모션이 끝날 때까지 따라가기나 배회 명령을 멈춰 공격 자세가 이동으로 덮이지 않게 한다.
	if (bIsAttackAnimationActive)
	{
		if (GetServerTimeSeconds() < AttackAnimationEndServerTimeSeconds)
		{
			SendIdleSnapshot(*PokemonCharacter);
			return;
		}

		bIsAttackAnimationActive = false;
	}

	// 필드 행동 재생 중에는 새 이동 경로를 계산하지 않고 현재 위치를 유지한다.
	// 클라이언트는 최초 이벤트에서 받은 Start / Loop / End 대기열을 끝까지 재생한다.
	if (bIsFieldAnimationActive)
	{
		if (GetServerTimeSeconds() < FieldAnimationEndServerTimeSeconds)
		{
			SendIdleSnapshot(*PokemonCharacter);
			return;
		}

		bIsFieldAnimationActive = false;
	}

	if (SimulationMode == EUEPokemonServerSimulationMode::Wander)
	{
		RunWanderSimulationTick(*PokemonCharacter, DeltaSeconds);
		return;
	}

	RunFollowSimulationTick(*PokemonCharacter, DeltaSeconds);
}

void UUEPokemonServerComponent::RunFollowSimulationTick(AUEPokemonCharacter& PokemonCharacter, float DeltaSeconds)
{
	const AActor* CurrentFollowTargetActor = ResolveFollowTargetActor();
	if (!CurrentFollowTargetActor)
	{
		return;
	}

	const HHV::PokemonAI::OwnContext Context = MakeOwnContext(PokemonCharacter, *CurrentFollowTargetActor, DeltaSeconds);
	const HHV::PokemonAI::Command Command = ServerBrain.Tick(Context);
	ApplyServerCommand(PokemonCharacter, Command, DeltaSeconds);
}

void UUEPokemonServerComponent::RunWanderSimulationTick(AUEPokemonCharacter& PokemonCharacter, float DeltaSeconds)
{
	const HHV::PokemonAI::WildContext Context = MakeWildContext(PokemonCharacter, DeltaSeconds);
	const HHV::PokemonAI::Command Command = WildBrain.Tick(Context);
	ApplyServerCommand(PokemonCharacter, Command, DeltaSeconds);
}

void UUEPokemonServerComponent::ApplyServerCommand(AUEPokemonCharacter& PokemonCharacter, const HHV::PokemonAI::Command& Command, float DeltaSeconds)
{
	using HHV::PokemonAI::CommandType;

	switch (Command.Type)
	{
	case CommandType::MoveTo:
	{
		const FVector TargetLocation = ToUnrealVector(Command.TargetLocation);
		const FVector ToTarget = TargetLocation - ServerSimulatedLocation;
		const float DistanceToTarget = ToTarget.Size();
		const float AcceptanceRadius = FMath::Max(Command.AcceptanceRadius, 1.0f);
		// 야생 포켓몬은 산책마다 속도 배율을 바꾼다. 소유 포켓몬처럼 배율을 지정하지 않은 명령은 1배다.
		const float MoveSpeed = ResolveServerMoveSpeed(PokemonCharacter)
			* FMath::Clamp(Command.MoveSpeedScale, 0.1f, 2.0f);
		if (DistanceToTarget <= AcceptanceRadius)
		{
			ServerSimulatedLocation = TargetLocation;
			FVector NextPathTarget;
			if (TryFindNextPathTarget(Command, TargetLocation, AcceptanceRadius, NextPathTarget))
			{
				const FVector ToNextTarget = NextPathTarget - ServerSimulatedLocation;
				const FVector MoveDirection = ToNextTarget.GetSafeNormal();
				ServerSimulatedVelocity = MoveDirection * MoveSpeed;
				if (!MoveDirection.IsNearlyZero())
				{
					ServerSimulatedRotation = MoveDirection.ToOrientationRotator();
				}
			}
			else
			{
				ServerSimulatedVelocity = FVector::ZeroVector;
			}
		}
		else
		{
			const FVector MoveDirection = ToTarget / DistanceToTarget;
			const float MoveDistance = FMath::Min(MoveSpeed * DeltaSeconds, DistanceToTarget);
			ServerSimulatedLocation += MoveDirection * MoveDistance;
			ServerSimulatedVelocity = MoveDirection * MoveSpeed;
			ServerSimulatedRotation = MoveDirection.ToOrientationRotator();
		}

		// The local server sends the same movement snapshot the real server will send later.
		const EUEPokemonAnimationState AnimationState = ServerSimulatedVelocity.IsNearlyZero()
			? EUEPokemonAnimationState::Idle
			: EUEPokemonAnimationState::Moving;
		SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, false, ResolveAnimationState(AnimationState), EUEPokemonAnimationEvent::None, 0.0f);
		break;
	}
	case CommandType::PlayFieldAnimation:
		if (!TryStartRandomFieldAnimation(PokemonCharacter))
		{
			SendIdleSnapshot(PokemonCharacter);
		}
		break;
	case CommandType::Teleport:
		ServerSimulatedLocation = ToUnrealVector(Command.TargetLocation);
		ServerSimulatedVelocity = FVector::ZeroVector;
		SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, true, ResolveAnimationState(EUEPokemonAnimationState::Idle), EUEPokemonAnimationEvent::None, 0.0f);
		break;
	case CommandType::FaceTarget:
	{
		FVector ToTarget = ToUnrealVector(Command.TargetLocation) - ServerSimulatedLocation;
		ToTarget.Z = 0.0f;
		ServerSimulatedVelocity = FVector::ZeroVector;
		if (!ToTarget.IsNearlyZero())
		{
			ServerSimulatedRotation = ToTarget.ToOrientationRotator();
		}
		const EUEPokemonAnimationState AnimationState = ToTarget.IsNearlyZero()
			? EUEPokemonAnimationState::Idle
			: EUEPokemonAnimationState::Turning;
		SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, false, ResolveAnimationState(AnimationState), EUEPokemonAnimationEvent::None, 0.0f);
		break;
	}
	case CommandType::Stop:
	case CommandType::None:
	default:
		ServerSimulatedVelocity = FVector::ZeroVector;
		SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, false, ResolveAnimationState(EUEPokemonAnimationState::Idle), EUEPokemonAnimationEvent::None, 0.0f);
		break;
	}
}

void UUEPokemonServerComponent::SendServerSnapshot(AUEPokemonCharacter& PokemonCharacter, const FVector& Location, const FVector& Velocity, const FRotator& Rotation, bool bTeleported, EUEPokemonAnimationState AnimationState, EUEPokemonAnimationEvent AnimationEvent, float EventDurationSeconds, EUEPokemonFieldAnimation FieldAnimation, int32 FieldAnimationLoopCount, EUEPokemonAttackAnimation AttackAnimation, int32 AttackAnimationLoopCount) const
{
	FUEPokemonServerMoveSnapshot Snapshot;
	Snapshot.PokemonId = ServerPokemonId;
	Snapshot.PokemonInstanceId = ServerPokemonInstanceId;
	Snapshot.SpeciesId = PokemonCharacter.GetPokemonSpeciesId();
	Snapshot.RenderType = PokemonCharacter.GetRenderType();
	Snapshot.MaxHP = ServerMaxHP > 0.0f ? ServerMaxHP : PokemonCharacter.GetMaxHP();
	Snapshot.CurrentHP = ServerMaxHP > 0.0f ? FMath::Clamp(ServerCurrentHP, 0.0f, Snapshot.MaxHP) : PokemonCharacter.GetCurrentHP();
	Snapshot.Location = Location;
	Snapshot.Velocity = Velocity;
	Snapshot.Rotation = Rotation;
	Snapshot.bTeleported = bTeleported;
	Snapshot.AnimationState = AnimationState;
	Snapshot.AnimationEvent = AnimationEvent;
	Snapshot.FieldAnimation = FieldAnimation;
	Snapshot.FieldAnimationLoopCount = FMath::Max(FieldAnimationLoopCount, 1);
	Snapshot.AttackAnimation = AttackAnimation;
	Snapshot.AttackAnimationLoopCount = FMath::Max(AttackAnimationLoopCount, 1);
	Snapshot.ServerTimeSeconds = GetServerTimeSeconds();
	Snapshot.EventDurationSeconds = EventDurationSeconds;
	PokemonCharacter.ApplyServerMoveSnapshot(Snapshot);
}

EUEPokemonAnimationState UUEPokemonServerComponent::ResolveAnimationState(EUEPokemonAnimationState FallbackState)
{
	if (!bHasForcedAnimationState)
	{
		return FallbackState;
	}

	if (GetServerTimeSeconds() <= ForcedAnimationStateEndServerTimeSeconds)
	{
		return ForcedAnimationState;
	}

	bHasForcedAnimationState = false;
	return FallbackState;
}

float UUEPokemonServerComponent::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0f;
}

float UUEPokemonServerComponent::ResolveServerMoveSpeed(const AUEPokemonCharacter& PokemonCharacter) const
{
	const float SpeciesMoveSpeed = PokemonCharacter.GetConfiguredMoveSpeed();
	if (SpeciesMoveSpeed <= 0.0f)
	{
		return ServerMoveSpeed;
	}

	// 야생은 외부 FieldServer의 wildMoveSpeed와 같은 종별 원속도를 사용한다.
	// 로컬 테스트에서만 3배가 적용되면 서버 야생과 섞였을 때 일부 개체가 유난히
	// 빠르게 보이고, 같은 애니메이션의 발 보폭도 서로 달라진다.
	if (PokemonCharacter.GetRenderType() == EUEPokemonRenderType::Wild)
	{
		return SpeciesMoveSpeed;
	}

	// 소유 포켓몬은 플레이어를 따라잡아야 하므로 기존 추종용 배율을 유지한다.
	return SpeciesMoveSpeed * SpeciesMoveSpeedToUnrealUnits;
}

void UUEPokemonServerComponent::SendIdleSnapshot(AUEPokemonCharacter& PokemonCharacter)
{
	ServerSimulatedVelocity = FVector::ZeroVector;
	SendServerSnapshot(PokemonCharacter, ServerSimulatedLocation, ServerSimulatedVelocity, ServerSimulatedRotation, false, ResolveAnimationState(EUEPokemonAnimationState::Idle), EUEPokemonAnimationEvent::None, 0.0f);
}

bool UUEPokemonServerComponent::TryStartRandomFieldAnimation(AUEPokemonCharacter& PokemonCharacter)
{
	const UUEPokemonSpeciesData* SpeciesData = PokemonCharacter.GetPokemonSpeciesData();
	if (!SpeciesData)
	{
		return false;
	}

	if (FieldAnimationBag.IsEmpty())
	{
		// 현재 종에 실제 시퀀스가 하나라도 있는 행동만 셔플 주머니에 넣는다.
		// 주머니가 빌 때까지 같은 행동을 다시 넣지 않으므로 등록된 행동이 고르게 사용된다.
		const auto AddSingle = [this](EUEPokemonFieldAnimation Type, const UAnimSequence* Sequence)
		{
			if (Sequence)
			{
				FieldAnimationBag.Add(Type);
			}
		};

		const auto AddChain = [this](EUEPokemonFieldAnimation Type, const UAnimSequence* Start, const UAnimSequence* Loop, const UAnimSequence* End)
		{
			if (Start || Loop || End)
			{
				FieldAnimationBag.Add(Type);
			}
		};

		AddSingle(EUEPokemonFieldAnimation::Idle01, SpeciesData->Idle01);
		AddSingle(EUEPokemonFieldAnimation::Idle02, SpeciesData->Idle02);
		AddSingle(EUEPokemonFieldAnimation::TurnLeft90, SpeciesData->TurnLeft90);
		AddSingle(EUEPokemonFieldAnimation::TurnRight90, SpeciesData->TurnRight90);
		AddChain(EUEPokemonFieldAnimation::Eat01, SpeciesData->Eat01Start, SpeciesData->Eat01Loop, SpeciesData->Eat01End);
		AddChain(EUEPokemonFieldAnimation::Eat02, SpeciesData->Eat02Start, SpeciesData->Eat02Loop, SpeciesData->Eat02End);
		AddChain(EUEPokemonFieldAnimation::Sleep, SpeciesData->SleepStart, SpeciesData->SleepLoop, SpeciesData->SleepEnd);
		AddChain(EUEPokemonFieldAnimation::Rest, SpeciesData->RestStart, SpeciesData->RestLoop, SpeciesData->RestEnd);
		AddSingle(EUEPokemonFieldAnimation::Notice, SpeciesData->Notice);
		AddSingle(EUEPokemonFieldAnimation::Roar, SpeciesData->Roar);
		AddSingle(EUEPokemonFieldAnimation::Glad, SpeciesData->Glad);
		AddSingle(EUEPokemonFieldAnimation::Hate, SpeciesData->Hate);
		AddSingle(EUEPokemonFieldAnimation::Refresh, SpeciesData->Refresh);
		AddChain(EUEPokemonFieldAnimation::StepOut, SpeciesData->StepOutStart, SpeciesData->StepOut, SpeciesData->StepOutEnd);
	}

	while (!FieldAnimationBag.IsEmpty())
	{
		const int32 SelectedIndex = FieldAnimationRandomStream.RandRange(0, FieldAnimationBag.Num() - 1);
		const EUEPokemonFieldAnimation SelectedAnimation = FieldAnimationBag[SelectedIndex];
		FieldAnimationBag.RemoveAtSwap(SelectedIndex);

		const int32 MinLoopCount = FMath::Clamp(FMath::Min(FieldAnimationMinLoopCount, FieldAnimationMaxLoopCount), 1, 10);
		const int32 MaxLoopCount = FMath::Clamp(FMath::Max(FieldAnimationMinLoopCount, FieldAnimationMaxLoopCount), MinLoopCount, 10);
		const int32 LoopCount = FieldAnimationRandomStream.RandRange(MinLoopCount, MaxLoopCount);
		const float AnimationDuration = GetFieldAnimationDuration(*SpeciesData, SelectedAnimation, LoopCount);
		if (AnimationDuration <= 0.0f)
		{
			continue;
		}

		// 행동이 끝날 때까지 서버 이동을 멈추고 최초 한 번만 재생 이벤트를 보낸다.
		ServerSimulatedVelocity = FVector::ZeroVector;
		bIsFieldAnimationActive = true;
		FieldAnimationEndServerTimeSeconds = GetServerTimeSeconds() + AnimationDuration + 0.25f;
		SendServerSnapshot(
			PokemonCharacter,
			ServerSimulatedLocation,
			ServerSimulatedVelocity,
			ServerSimulatedRotation,
			false,
			ResolveAnimationState(EUEPokemonAnimationState::Idle),
			EUEPokemonAnimationEvent::FieldAnimationStarted,
			AnimationDuration,
			SelectedAnimation,
			LoopCount
		);
		return true;
	}

	return false;
}

void UUEPokemonServerComponent::ResetWildState()
{
	WildBrain.Reset();
	FieldAnimationBag.Reset();
	bIsFieldAnimationActive = false;
	FieldAnimationEndServerTimeSeconds = 0.0f;
	bIsAttackAnimationActive = false;
	AttackAnimationEndServerTimeSeconds = 0.0f;
}

AUEPokemonCharacter* UUEPokemonServerComponent::GetPokemonOwner() const
{
	return Cast<AUEPokemonCharacter>(GetOwner());
}

AActor* UUEPokemonServerComponent::ResolveFollowTargetActor() const
{
	// 따라갈 대상은 반드시 명시돼야 한다. 예전에는 비어 있으면 첫 플레이어 폰으로
	// 넘어갔는데, 그러면 이 컴포넌트를 단 액터가 **누구든** 조용히 플레이어를
	// 따라온다. 기본값이 bEnableServer=true / FollowOwner 라, 서버가 좌표를
	// 지시하는 야생 포켓몬까지 전부 플레이어에게 몰려와 겹쳐 쌓였다.
	// 동행 포켓몬은 SetFollowTargetActor 로 주인을 직접 넣는다.
	return FollowTargetActor;
}

HHV::PokemonAI::OwnContext UUEPokemonServerComponent::MakeOwnContext(const AUEPokemonCharacter& PokemonCharacter, const AActor& CurrentFollowTargetActor, float DeltaSeconds) const
{
	HHV::PokemonAI::OwnContext Context;
	Context.DeltaSeconds = DeltaSeconds;
	Context.PokemonLocation = ToServerVec3(ServerSimulatedLocation);
	Context.OwnerLocation = ToServerVec3(CurrentFollowTargetActor.GetActorLocation());
	Context.OwnerYawDegrees = GetFollowTargetYawDegrees(CurrentFollowTargetActor);
	Context.ActionRequest = HHV::PokemonAI::RequestedAction::FollowOwner;
	Context.ServerMap = bServerMapLoaded ? &ServerMapRuntime : nullptr;
	Context.Agent = MakeAgentSettings(PokemonCharacter);
	return Context;
}

HHV::PokemonAI::WildContext UUEPokemonServerComponent::MakeWildContext(const AUEPokemonCharacter& PokemonCharacter, float DeltaSeconds) const
{
	HHV::PokemonAI::WildContext Context;
	Context.DeltaSeconds = DeltaSeconds;
	Context.PokemonLocation = ToServerVec3(ServerSimulatedLocation);
	Context.ServerMap = bServerMapLoaded ? &ServerMapRuntime : nullptr;
	Context.Agent = MakeAgentSettings(PokemonCharacter);
	Context.WanderWaitSeconds = WanderWaitSeconds;
	Context.WanderWaitVariationSeconds = WanderWaitVariationSeconds;
	Context.WanderMinDistance = WanderMinDistance;
	Context.WanderMaxDistance = WanderMaxDistance;
	Context.WanderHomeRadius = WanderHomeRadius;
	Context.WanderMinMoveSpeedScale = WanderMinMoveSpeedScale;
	Context.WanderMaxMoveSpeedScale = WanderMaxMoveSpeedScale;
	Context.WanderLookAroundChance = WanderLookAroundChance;
	Context.WanderFieldAnimationChance = WanderFieldAnimationChance;
	Context.WanderAcceptanceRadius = WanderAcceptanceRadius;
	Context.WanderSearchAttempts = WanderSearchAttempts;
	return Context;
}

HHV::Map::AgentSettings UUEPokemonServerComponent::MakeAgentSettings(const AUEPokemonCharacter& PokemonCharacter) const
{
	HHV::Map::AgentSettings Agent;

	if (const UCapsuleComponent* CapsuleComponent = PokemonCharacter.GetCapsuleComponent())
	{
		Agent.CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
		Agent.CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	}

	if (const UCharacterMovementComponent* MovementComponent = PokemonCharacter.GetCharacterMovement())
	{
		Agent.MaxStepHeight = MovementComponent->MaxStepHeight;
		Agent.WalkableFloorAngleDegrees = MovementComponent->GetWalkableFloorAngle();
	}

	return Agent;
}

FString UUEPokemonServerComponent::ResolveServerMapFilePath() const
{
	if (!ServerMapFilePath.IsEmpty())
	{
		return ServerMapFilePath;
	}

	if (!bTryLoadDefaultServerMap || DefaultServerMapFileName.IsEmpty())
	{
		return FString();
	}

	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("ServerMaps"),
		DefaultServerMapFileName
	);
}

float UUEPokemonServerComponent::GetFollowTargetYawDegrees(const AActor& CurrentFollowTargetActor) const
{
	// Own Pokemon placement follows the character body yaw, not the camera/control yaw.
	return CurrentFollowTargetActor.GetActorRotation().Yaw;
}

HHV::Map::Vec3 UUEPokemonServerComponent::ToServerVec3(const FVector& Vector)
{
	return HHV::Map::Vec3{
		static_cast<float>(Vector.X),
		static_cast<float>(Vector.Y),
		static_cast<float>(Vector.Z)
	};
}

FVector UUEPokemonServerComponent::ToUnrealVector(const HHV::Map::Vec3& Vector)
{
	return FVector(Vector.X, Vector.Y, Vector.Z);
}
