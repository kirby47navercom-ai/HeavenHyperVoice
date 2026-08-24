// Fill out your copyright notice in the Description page of Project Settings.

#include "UEAnimInstance.h"

#include "../Character/UEPlayerCharacter.h"
#include "../UEGameplayTags.h"

#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"

void UUEAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwnerCharacter = Cast<AUEPlayerCharacter>(TryGetPawnOwner());
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

float UUEAnimInstance::PlayRollMontage(UAnimSequenceBase* RollAnimation)
{
	if (!RollAnimation)
	{
		return 0.0f;
	}

	UAnimMontage* RollMontage = PlaySlotAnimationAsDynamicMontage(
		RollAnimation,
		TEXT("FullBodySlot"),
		0.1f,
		0.1f,
		1.0f,
		1);
	return RollMontage ? RollMontage->GetPlayLength() : 0.0f;
}

void UUEAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<AUEPlayerCharacter>(TryGetPawnOwner());
	}

	if (!OwnerCharacter)
	{
		GroundSpeed = 0.0f;
		DirectionAngle = 0.0f;
		MovementInput = FVector2D::ZeroVector;
		CharacterStateTag = FGameplayTag::EmptyTag;
		bIsMoving = false;
		bIsRunning = false;
		bIsRolling = false;
		bIsFalling = false;
		bIsIdle = false;
		bIsWalking = false;
		bIsJumping = false;
		bIsLanding = false;
		bIsHolding = false;
		bIsThrowing = false;
		bIsTakingDamage = false;
		bIsDead = false;
		bIsFemale = false;
		bIsMale = false;
		return;
	}

	const FVector Velocity = OwnerCharacter->GetVelocity();
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);
	const UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement();
	const bool bMovementIsFalling = MovementComponent && MovementComponent->IsFalling();
	const bool bWasMovementFalling = bIsJumping || bIsFalling;
	GroundSpeed = HorizontalVelocity.Size();
	MovementInput = OwnerCharacter->GetMovementInput();
	CharacterStateTag = OwnerCharacter->GetCharacterStateTag();

	bIsIdle = CharacterStateTag == UEGameplayTags::State_Character_Idle;
	bIsWalking = CharacterStateTag == UEGameplayTags::State_Character_Walk;
	bIsRunning = CharacterStateTag == UEGameplayTags::State_Character_Run;
	bIsRolling = CharacterStateTag == UEGameplayTags::State_Character_Roll;
	// 점프 상태는 태그 갱신 시점이 아니라 실제 CharacterMovement 공중 상태를 기준으로 판정한다.
	bIsJumping = bMovementIsFalling && Velocity.Z > 0.0f;
	bIsFalling = bMovementIsFalling && Velocity.Z <= 0.0f;
	bIsLanding = !bMovementIsFalling &&
		(bWasMovementFalling || CharacterStateTag == UEGameplayTags::State_Character_Landing);
	bIsHolding = CharacterStateTag == UEGameplayTags::State_Character_Holding;
	bIsThrowing = CharacterStateTag == UEGameplayTags::State_Character_Throw;
	bIsTakingDamage = CharacterStateTag == UEGameplayTags::State_Character_Damage;
	bIsDead = CharacterStateTag == UEGameplayTags::State_Character_Death;
	bIsMoving = bIsWalking || bIsRunning;
	bIsFemale = OwnerCharacter->GetCustomizationGender() == EUEHHVGender::TypeA;
	bIsMale = OwnerCharacter->GetCustomizationGender() == EUEHHVGender::TypeB;

	if (HorizontalVelocity.IsNearlyZero())
	{
		DirectionAngle = 0.0f;
		return;
	}

	const FVector ActorForward = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	const FVector MoveDirection = HorizontalVelocity.GetSafeNormal2D();
	const double Dot = FVector::DotProduct(ActorForward, MoveDirection);
	const double CrossZ = FVector::CrossProduct(ActorForward, MoveDirection).Z;
	DirectionAngle = FMath::RadiansToDegrees(FMath::Atan2(CrossZ, Dot));
}
