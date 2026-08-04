// Fill out your copyright notice in the Description page of Project Settings.


#include "UEAnimInstance.h"

#include "../Character/UEPlayerCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"

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
		bIsMoving = false;
		bIsRunning = false;
		bIsRolling = false;
		bIsFalling = false;
		return;
	}

	const FVector Velocity = OwnerCharacter->GetVelocity();
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);
	GroundSpeed = HorizontalVelocity.Size();
	MovementInput = OwnerCharacter->GetMovementInput();
	bIsMoving = GroundSpeed > 3.0f;
	bIsRunning = OwnerCharacter->IsRunning();
	bIsRolling = OwnerCharacter->IsRolling();

	const UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement();
	bIsFalling = MovementComponent ? MovementComponent->IsFalling() : false;

	const FVector ActorForward = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	const FVector MoveDirection = HorizontalVelocity.GetSafeNormal2D();
	const double Dot = FVector::DotProduct(ActorForward, MoveDirection);
	const double CrossZ = FVector::CrossProduct(ActorForward, MoveDirection).Z;
	DirectionAngle = FMath::RadiansToDegrees(FMath::Atan2(CrossZ, Dot));
}
