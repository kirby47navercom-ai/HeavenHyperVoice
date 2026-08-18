#include "UEPokemonAnimInstance.h"

#include "../Pokemon/UEPokemonCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"

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
		DirectionAngle = 0.0f;
		bIsMoving = false;
		bIsRunning = false;
		bIsFalling = false;
		return;
	}

	const FVector Velocity = OwnerPokemon->GetVelocity();
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);
	GroundSpeed = HorizontalVelocity.Size();
	bIsMoving = GroundSpeed > 3.0f;
	bIsRunning = GroundSpeed >= RunSpeedThreshold;

	const UCharacterMovementComponent* MovementComponent = OwnerPokemon->GetCharacterMovement();
	bIsFalling = MovementComponent ? MovementComponent->IsFalling() : false;

	const FVector ActorForward = OwnerPokemon->GetActorForwardVector().GetSafeNormal2D();
	const FVector MoveDirection = HorizontalVelocity.GetSafeNormal2D();
	const double Dot = FVector::DotProduct(ActorForward, MoveDirection);
	const double CrossZ = FVector::CrossProduct(ActorForward, MoveDirection).Z;
	DirectionAngle = FMath::RadiansToDegrees(FMath::Atan2(CrossZ, Dot));
}
