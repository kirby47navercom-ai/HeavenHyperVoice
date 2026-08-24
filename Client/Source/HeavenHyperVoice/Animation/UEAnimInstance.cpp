// Fill out your copyright notice in the Description page of Project Settings.

#include "UEAnimInstance.h"

#include "../Character/UEPlayerCharacter.h"
#include "../UEGameplayTags.h"

void UUEAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwnerCharacter = Cast<AUEPlayerCharacter>(TryGetPawnOwner());
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
	GroundSpeed = HorizontalVelocity.Size();
	MovementInput = OwnerCharacter->GetMovementInput();
	CharacterStateTag = OwnerCharacter->GetCharacterStateTag();

	bIsIdle = CharacterStateTag == UEGameplayTags::State_Character_Idle;
	bIsWalking = CharacterStateTag == UEGameplayTags::State_Character_Walk;
	bIsRunning = CharacterStateTag == UEGameplayTags::State_Character_Run;
	bIsRolling = CharacterStateTag == UEGameplayTags::State_Character_Roll;
	bIsJumping = CharacterStateTag == UEGameplayTags::State_Character_Jump;
	bIsFalling = CharacterStateTag == UEGameplayTags::State_Character_Fall;
	bIsLanding = CharacterStateTag == UEGameplayTags::State_Character_Landing;
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
