// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPlayerCharacter.h"

#include "../Component/UEPlayerMovementSyncComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

AUEPlayerCharacter::AUEPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
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
}

void AUEPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AUEPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyLocalMovementInput();
}

FVector AUEPlayerCharacter::GetDesiredMovementDirection() const
{
	return GetMoveDirectionFromInput(MovementInput, GetControlRotation());
}

void AUEPlayerCharacter::SetMovementInput(const FVector2D& NewMovementInput)
{
	MovementInput = NewMovementInput.GetClampedToMaxSize(1.0f);
}

void AUEPlayerCharacter::ApplyLocalMovementInput()
{
	const FVector DesiredDirection = GetMoveDirectionFromInput(MovementInput, GetControlRotation());
	if (DesiredDirection.IsNearlyZero())
	{
		return;
	}

	// Movement follows camera yaw; idle camera rotation does not rotate the character.
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
	const FVector Direction = GetCameraForwardAxis(ViewRotation) * Input.X + GetCameraRightAxis(ViewRotation) * Input.Y;
	return Direction.GetSafeNormal();
}

void AUEPlayerCharacter::ApplyServerMovementCorrection(const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bUseHardCorrection)
{
	const ETeleportType CorrectionTeleportType = bUseHardCorrection ? ETeleportType::TeleportPhysics : ETeleportType::None;

	// Server correction updates the physical character state, then local movement continues next tick.
	SetActorLocation(ServerPosition, false, nullptr, CorrectionTeleportType);
	SetActorRotation(ServerRotation, CorrectionTeleportType);
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->Velocity = ServerVelocity;
	}
}
