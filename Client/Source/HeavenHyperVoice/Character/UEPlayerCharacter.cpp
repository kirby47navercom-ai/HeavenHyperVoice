// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPlayerCharacter.h"

#include "../Data/UEDataAsset.h"
#include "../UEGameplayTags.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"

AUEPlayerCharacter::AUEPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = JumpVelocity;
	GetCharacterMovement()->AirControl = AirControl;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 420.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AUEPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	RefreshMovementSpeed();
	AddDefaultMappingContext();
}

void AUEPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyMovementInput();
}

void AUEPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BindInputActions(PlayerInputComponent);
}

FVector AUEPlayerCharacter::GetDesiredMovementDirection() const
{
	const FVector Direction(MovementInput.X, MovementInput.Y, 0.0f);
	return Direction.GetSafeNormal();
}

void AUEPlayerCharacter::BindInputActions(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent || !InputData)
	{
		return;
	}

	const UInputAction* MoveForwardAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_MoveForward);
	const UInputAction* MoveBackwardAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_MoveBackward);
	const UInputAction* MoveRightAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_MoveRight);
	const UInputAction* MoveLeftAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_MoveLeft);
	const UInputAction* LookYawAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_LookYaw);
	const UInputAction* LookPitchAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_LookPitch);
	const UInputAction* RunAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_Run);
	const UInputAction* RollAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_Roll);
	const UInputAction* JumpAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_Jump);

	if (MoveForwardAction)
	{
		EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &ThisClass::HandleMoveForward);
		EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::Completed, this, &ThisClass::HandleMoveForwardCompleted);
	}

	if (MoveBackwardAction)
	{
		EnhancedInputComponent->BindAction(MoveBackwardAction, ETriggerEvent::Triggered, this, &ThisClass::HandleMoveBackward);
		EnhancedInputComponent->BindAction(MoveBackwardAction, ETriggerEvent::Completed, this, &ThisClass::HandleMoveBackwardCompleted);
	}

	if (MoveRightAction)
	{
		EnhancedInputComponent->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &ThisClass::HandleMoveRight);
		EnhancedInputComponent->BindAction(MoveRightAction, ETriggerEvent::Completed, this, &ThisClass::HandleMoveRightCompleted);
	}

	if (MoveLeftAction)
	{
		EnhancedInputComponent->BindAction(MoveLeftAction, ETriggerEvent::Triggered, this, &ThisClass::HandleMoveLeft);
		EnhancedInputComponent->BindAction(MoveLeftAction, ETriggerEvent::Completed, this, &ThisClass::HandleMoveLeftCompleted);
	}

	if (LookYawAction)
	{
		EnhancedInputComponent->BindAction(LookYawAction, ETriggerEvent::Triggered, this, &ThisClass::HandleLookYaw);
	}

	if (LookPitchAction)
	{
		EnhancedInputComponent->BindAction(LookPitchAction, ETriggerEvent::Triggered, this, &ThisClass::HandleLookPitch);
	}

	if (RunAction)
	{
		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Started, this, &ThisClass::HandleRunStarted);
		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Completed, this, &ThisClass::HandleRunCompleted);
	}

	if (RollAction)
	{
		EnhancedInputComponent->BindAction(RollAction, ETriggerEvent::Started, this, &ThisClass::HandleRollStarted);
	}

	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::HandleJumpStarted);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ThisClass::HandleJumpCompleted);
	}
}

void AUEPlayerCharacter::AddDefaultMappingContext() const
{
	if (!InputData || !InputData->InputMappingContext)
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (InputSubsystem)
	{
		InputSubsystem->AddMappingContext(InputData->InputMappingContext, 0);
	}
}

void AUEPlayerCharacter::ApplyMovementInput()
{
	if (bIsRolling)
	{
		AddMovementInput(RollDirection, 1.0f);
		return;
	}

	// Project convention:
	// W increases world +X, S decreases world X.
	// D increases world +Y, A decreases world Y.
	const FVector ForwardAxis = FVector::ForwardVector;
	const FVector RightAxis = FVector::RightVector;

	AddMovementInput(ForwardAxis, MovementInput.X);
	AddMovementInput(RightAxis, MovementInput.Y);
}

void AUEPlayerCharacter::RefreshMovementSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = bIsRolling ? RollSpeed : (bIsRunning ? RunSpeed : WalkSpeed);
	GetCharacterMovement()->JumpZVelocity = JumpVelocity;
	GetCharacterMovement()->AirControl = AirControl;
}

void AUEPlayerCharacter::HandleMoveForward(const FInputActionValue& Value)
{
	MovementInput.X = Value.Get<float>();
}

void AUEPlayerCharacter::HandleMoveForwardCompleted(const FInputActionValue& Value)
{
	MovementInput.X = 0.0f;
}

void AUEPlayerCharacter::HandleMoveBackward(const FInputActionValue& Value)
{
	MovementInput.X = -Value.Get<float>();
}

void AUEPlayerCharacter::HandleMoveBackwardCompleted(const FInputActionValue& Value)
{
	if (MovementInput.X < 0.0f)
	{
		MovementInput.X = 0.0f;
	}
}

void AUEPlayerCharacter::HandleMoveRight(const FInputActionValue& Value)
{
	MovementInput.Y = Value.Get<float>();
}

void AUEPlayerCharacter::HandleMoveRightCompleted(const FInputActionValue& Value)
{
	MovementInput.Y = 0.0f;
}

void AUEPlayerCharacter::HandleMoveLeft(const FInputActionValue& Value)
{
	MovementInput.Y = -Value.Get<float>();
}

void AUEPlayerCharacter::HandleMoveLeftCompleted(const FInputActionValue& Value)
{
	if (MovementInput.Y < 0.0f)
	{
		MovementInput.Y = 0.0f;
	}
}

void AUEPlayerCharacter::HandleLookYaw(const FInputActionValue& Value)
{
	AddControllerYawInput(Value.Get<float>() * LookYawRate);
}

void AUEPlayerCharacter::HandleLookPitch(const FInputActionValue& Value)
{
	AddControllerPitchInput(Value.Get<float>() * LookPitchRate);
}

void AUEPlayerCharacter::HandleRunStarted(const FInputActionValue& Value)
{
	bIsRunning = true;
	RefreshMovementSpeed();
}

void AUEPlayerCharacter::HandleRunCompleted(const FInputActionValue& Value)
{
	bIsRunning = false;
	RefreshMovementSpeed();
}

void AUEPlayerCharacter::HandleRollStarted(const FInputActionValue& Value)
{
	if (bIsRolling || GetWorld()->GetTimeSeconds() < LastRollEndTime + RollCooldown)
	{
		return;
	}

	RollDirection = GetDesiredMovementDirection();
	if (RollDirection.IsNearlyZero())
	{
		RollDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	bIsRolling = true;
	RefreshMovementSpeed();
	GetWorldTimerManager().SetTimer(RollTimerHandle, this, &ThisClass::FinishRoll, RollDuration, false);
}

void AUEPlayerCharacter::HandleJumpStarted(const FInputActionValue& Value)
{
	if (!bIsRolling)
	{
		Jump();
	}
}

void AUEPlayerCharacter::HandleJumpCompleted(const FInputActionValue& Value)
{
	StopJumping();
}

void AUEPlayerCharacter::FinishRoll()
{
	bIsRolling = false;
	LastRollEndTime = GetWorld()->GetTimeSeconds();
	RefreshMovementSpeed();
}
