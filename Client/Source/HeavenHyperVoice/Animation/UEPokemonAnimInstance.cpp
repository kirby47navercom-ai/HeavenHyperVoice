#include "UEPokemonAnimInstance.h"

#include "../Pokemon/UEPokemonCharacter.h"
#include "../Pokemon/UEPokemonSpeciesData.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

void UUEPokemonAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerPokemon = Cast<AUEPokemonCharacter>(TryGetPawnOwner());
}

void UUEPokemonAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerPokemon)
	{
		OwnerPokemon = Cast<AUEPokemonCharacter>(TryGetPawnOwner());
	}

	if (!OwnerPokemon)
	{
		GroundSpeed = 0.0f;
		ForwardSpeed = 0.0f;
		DirectionAngle = 0.0f;
		LocomotionPlayRate = 1.0f;
		bIsMoving = false;
		bIsRunning = false;
		bIsFalling = false;
		return;
	}

	const FVector Velocity = OwnerPokemon->GetVelocity();
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);
	const FVector ActorForward = OwnerPokemon->GetActorForwardVector().GetSafeNormal2D();

	GroundSpeed = HorizontalVelocity.Size();
	ForwardSpeed = FVector::DotProduct(HorizontalVelocity, ActorForward);
	bIsMoving = GroundSpeed > 3.0f;

	// BlendSpace의 마지막 샘플보다 빨라질 때만 재생 속도를 올린다.
	// 마지막 샘플 안에서는 BlendSpace 좌표와 실제 속도가 같아서 발 미끄러짐이 줄어든다.
	const float EffectiveWalkSpeed = FMath::Max(WalkAnimationSpeed, 1.0f);
	const float EffectiveRunSpeed = FMath::Max(RunAnimationSpeed, EffectiveWalkSpeed + 1.0f);
	LocomotionPlayRate = GroundSpeed > EffectiveRunSpeed
		? FMath::Clamp(GroundSpeed / EffectiveRunSpeed, 1.0f, FMath::Max(MaxLocomotionPlayRate, 1.0f))
		: 1.0f;
	bIsRunning = GroundSpeed >= (EffectiveWalkSpeed + EffectiveRunSpeed) * 0.5f;

	const UCharacterMovementComponent* MovementComponent = OwnerPokemon->GetCharacterMovement();
	bIsFalling = MovementComponent ? MovementComponent->IsFalling() : false;

	const FVector MoveDirection = HorizontalVelocity.GetSafeNormal2D();
	const double Dot = FVector::DotProduct(ActorForward, MoveDirection);
	const double CrossZ = FVector::CrossProduct(ActorForward, MoveDirection).Z;
	DirectionAngle = FMath::RadiansToDegrees(FMath::Atan2(CrossZ, Dot));
}

void UUEPokemonAnimInstance::NativeUninitializeAnimation()
{
	StopFieldAnimation(0.0f);
	OwnerPokemon = nullptr;

	Super::NativeUninitializeAnimation();
}

bool UUEPokemonAnimInstance::PlayFieldAnimation(EUEPokemonFieldAnimation FieldAnimation, int32 LoopCount)
{
	if (!OwnerPokemon)
	{
		OwnerPokemon = Cast<AUEPokemonCharacter>(TryGetPawnOwner());
	}

	const UUEPokemonSpeciesData* SpeciesData = OwnerPokemon ? OwnerPokemon->GetPokemonSpeciesData() : nullptr;
	if (!SpeciesData || FieldAnimation == EUEPokemonFieldAnimation::None)
	{
		return false;
	}

	// 이전 행동이 남아 있으면 새 서버 명령을 우선하기 위해 부드럽게 종료한다.
	StopCurrentActionAnimation(FieldAnimationBlendOutTime);
	const int32 SafeLoopCount = FMath::Clamp(LoopCount, 1, 10);

	switch (FieldAnimation)
	{
	case EUEPokemonFieldAnimation::Idle01:
		QueueActionSequence(SpeciesData->Idle01);
		break;
	case EUEPokemonFieldAnimation::Idle02:
		QueueActionSequence(SpeciesData->Idle02);
		break;
	case EUEPokemonFieldAnimation::TurnLeft90:
		QueueActionSequence(SpeciesData->TurnLeft90);
		break;
	case EUEPokemonFieldAnimation::TurnRight90:
		QueueActionSequence(SpeciesData->TurnRight90);
		break;
	case EUEPokemonFieldAnimation::Eat01:
		QueueActionSequence(SpeciesData->Eat01Start);
		QueueActionSequence(SpeciesData->Eat01Loop, SafeLoopCount);
		QueueActionSequence(SpeciesData->Eat01End);
		break;
	case EUEPokemonFieldAnimation::Eat02:
		QueueActionSequence(SpeciesData->Eat02Start);
		QueueActionSequence(SpeciesData->Eat02Loop, SafeLoopCount);
		QueueActionSequence(SpeciesData->Eat02End);
		break;
	case EUEPokemonFieldAnimation::Sleep:
		QueueActionSequence(SpeciesData->SleepStart);
		QueueActionSequence(SpeciesData->SleepLoop, SafeLoopCount);
		QueueActionSequence(SpeciesData->SleepEnd);
		break;
	case EUEPokemonFieldAnimation::Rest:
		QueueActionSequence(SpeciesData->RestStart);
		QueueActionSequence(SpeciesData->RestLoop, SafeLoopCount);
		QueueActionSequence(SpeciesData->RestEnd);
		break;
	case EUEPokemonFieldAnimation::Notice:
		QueueActionSequence(SpeciesData->Notice);
		break;
	case EUEPokemonFieldAnimation::Roar:
		QueueActionSequence(SpeciesData->Roar);
		break;
	case EUEPokemonFieldAnimation::Glad:
		QueueActionSequence(SpeciesData->Glad);
		break;
	case EUEPokemonFieldAnimation::Hate:
		QueueActionSequence(SpeciesData->Hate);
		break;
	case EUEPokemonFieldAnimation::Refresh:
		QueueActionSequence(SpeciesData->Refresh);
		break;
	case EUEPokemonFieldAnimation::StepOut:
		QueueActionSequence(SpeciesData->StepOutStart);
		QueueActionSequence(SpeciesData->StepOut);
		QueueActionSequence(SpeciesData->StepOutEnd);
		break;
	case EUEPokemonFieldAnimation::None:
	default:
		break;
	}

	// 종별 DataAsset에 해당 행동이 하나도 없으면 Locomotion을 그대로 유지한다.
	if (ActionAnimationQueue.IsEmpty())
	{
		return false;
	}

	CurrentFieldAnimation = FieldAnimation;
	bIsFieldAnimationPlaying = true;
	PlayNextActionSequence();
	return bIsFieldAnimationPlaying;
}

bool UUEPokemonAnimInstance::PlayAttackAnimation(EUEPokemonAttackAnimation AttackAnimation, int32 LoopCount)
{
	if (!OwnerPokemon)
	{
		OwnerPokemon = Cast<AUEPokemonCharacter>(TryGetPawnOwner());
	}

	const UUEPokemonSpeciesData* SpeciesData = OwnerPokemon ? OwnerPokemon->GetPokemonSpeciesData() : nullptr;
	if (!SpeciesData || AttackAnimation == EUEPokemonAttackAnimation::None)
	{
		return false;
	}

	// 새 공격 명령은 남아 있는 필드 행동이나 이전 공격을 중단하고 가장 최근 명령을 우선한다.
	StopCurrentActionAnimation(FieldAnimationBlendOutTime);
	const int32 SafeLoopCount = FMath::Clamp(LoopCount, 1, 10);

	switch (AttackAnimation)
	{
	case EUEPokemonAttackAnimation::Attack01:
		QueueActionSequence(SpeciesData->Attack01);
		break;
	case EUEPokemonAttackAnimation::Attack02:
		QueueActionSequence(SpeciesData->Attack02);
		break;
	case EUEPokemonAttackAnimation::RangeAttack01:
		QueueActionSequence(SpeciesData->RangeAttack01);
		break;
	case EUEPokemonAttackAnimation::RangeAttack02:
		QueueActionSequence(SpeciesData->RangeAttack02Start);
		QueueActionSequence(SpeciesData->RangeAttack02Loop, SafeLoopCount);
		QueueActionSequence(SpeciesData->RangeAttack02End);
		break;
	case EUEPokemonAttackAnimation::None:
	default:
		break;
	}

	// 선택한 종에 해당 공격 시퀀스가 없으면 기본 이동 애니메이션을 유지한다.
	if (ActionAnimationQueue.IsEmpty())
	{
		return false;
	}

	CurrentAttackAnimation = AttackAnimation;
	bIsAttackAnimationPlaying = true;
	PlayNextActionSequence();
	return bIsAttackAnimationPlaying;
}

void UUEPokemonAnimInstance::StopFieldAnimation(float BlendOutTime)
{
	StopCurrentActionAnimation(BlendOutTime);
}

void UUEPokemonAnimInstance::StopAttackAnimation(float BlendOutTime)
{
	StopCurrentActionAnimation(BlendOutTime);
}

void UUEPokemonAnimInstance::StopCurrentActionAnimation(float BlendOutTime)
{
	// 먼저 공통 대기열과 상태를 비워 Montage 종료 콜백이 다음 조각을 재생하지 못하게 한다.
	ActionAnimationQueue.Reset();
	ActionAnimationLoopCounts.Reset();
	bIsFieldAnimationPlaying = false;
	CurrentFieldAnimation = EUEPokemonFieldAnimation::None;
	bIsAttackAnimationPlaying = false;
	CurrentAttackAnimation = EUEPokemonAttackAnimation::None;
	ActionSequenceStartRetryCount = 0;

	if (ActiveActionMontage)
	{
		UAnimMontage* MontageToStop = ActiveActionMontage;
		ActiveActionMontage = nullptr;
		Montage_Stop(FMath::Max(BlendOutTime, 0.0f), MontageToStop);
	}
}

void UUEPokemonAnimInstance::QueueActionSequence(UAnimSequence* Sequence, int32 LoopCount)
{
	// 종마다 없는 시퀀스가 다르므로 null은 대기열에 넣지 않는다.
	if (!Sequence)
	{
		return;
	}

	ActionAnimationQueue.Add(Sequence);
	ActionAnimationLoopCounts.Add(FMath::Clamp(LoopCount, 1, 10));
}

void UUEPokemonAnimInstance::PlayNextActionSequence()
{
	if ((!bIsFieldAnimationPlaying && !bIsAttackAnimationPlaying) || ActionAnimationQueue.IsEmpty())
	{
		ActiveActionMontage = nullptr;
		bIsFieldAnimationPlaying = false;
		CurrentFieldAnimation = EUEPokemonFieldAnimation::None;
		bIsAttackAnimationPlaying = false;
		CurrentAttackAnimation = EUEPokemonAttackAnimation::None;
		return;
	}

	UAnimSequence* Sequence = ActionAnimationQueue[0];
	const int32 LoopCount = ActionAnimationLoopCounts.IsValidIndex(0) ? ActionAnimationLoopCounts[0] : 1;
	ActiveActionMontage = PlaySlotAnimationAsDynamicMontage(
		Sequence,
		FieldAnimationSlotName,
		FieldAnimationBlendInTime,
		FieldAnimationBlendOutTime,
		1.0f,
		LoopCount
	);

	if (!ActiveActionMontage)
	{
		// TimerManager에서도 Slot 정리가 늦으면 현재 시퀀스를 보존한 채 제한된 횟수만 다시 시도한다.
		constexpr int32 MaxStartRetryCount = 5;
		++ActionSequenceStartRetryCount;
		if (ActionSequenceStartRetryCount > MaxStartRetryCount)
		{
			// 특정 시퀀스만 잘못되어도 나머지 Start / Loop / End 조각은 계속 재생할 수 있게 한다.
			ActionAnimationQueue.RemoveAt(0);
			if (!ActionAnimationLoopCounts.IsEmpty())
			{
				ActionAnimationLoopCounts.RemoveAt(0);
			}
			ActionSequenceStartRetryCount = 0;
		}

		ScheduleNextActionSequence();
		return;
	}

	// 재생이 성공한 뒤에만 대기열에서 제거하므로 재시도 중에도 행동 순서가 보존된다.
	ActionAnimationQueue.RemoveAt(0);
	if (!ActionAnimationLoopCounts.IsEmpty())
	{
		ActionAnimationLoopCounts.RemoveAt(0);
	}
	ActionSequenceStartRetryCount = 0;

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UUEPokemonAnimInstance::HandleActionMontageEnded);
	Montage_SetEndDelegate(EndDelegate, ActiveActionMontage);
}

void UUEPokemonAnimInstance::ScheduleNextActionSequence()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		StopFieldAnimation(0.0f);
		return;
	}

	// Montage 종료 콜백과 NativeUpdateAnimation 바깥인 다음 게임 프레임에서 Slot 재생을 시작한다.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (bIsFieldAnimationPlaying || bIsAttackAnimationPlaying)
		{
			PlayNextActionSequence();
		}
	}));
}

void UUEPokemonAnimInstance::HandleActionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != ActiveActionMontage)
	{
		return;
	}

	ActiveActionMontage = nullptr;
	if (bInterrupted)
	{
		ActionAnimationQueue.Reset();
		ActionAnimationLoopCounts.Reset();
		bIsFieldAnimationPlaying = false;
		CurrentFieldAnimation = EUEPokemonFieldAnimation::None;
		bIsAttackAnimationPlaying = false;
		CurrentAttackAnimation = EUEPokemonAttackAnimation::None;
		ActionSequenceStartRetryCount = 0;
		return;
	}

	ScheduleNextActionSequence();
}
