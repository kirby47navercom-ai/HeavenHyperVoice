// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPlayerController.h"

#include "../Character/UEPlayerCharacter.h"
#include "../CharacterCustomization/Palworld/Data/UEPalworldCustomizationTypes.h"
#include "../Data/UEDataAsset.h"
#include "../System/UEGameInstance.h"
#include "../UI/Login/UELoginWidget.h"
#include "../UEGameplayTags.h"

#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

AUEPlayerController::AUEPlayerController()
{
	LoginWidgetClass = UUELoginWidget::StaticClass();
	bShowMouseCursor = true;

	static ConstructorHelpers::FObjectFinder<UUEDataAsset> DefaultInputData(TEXT("/Game/Data/Input/DA_PlayerInput.DA_PlayerInput"));
	if (DefaultInputData.Succeeded())
	{
		InputData = DefaultInputData.Object;
	}
}

void AUEPlayerController::BeginPlay()
{
	Super::BeginPlay();

	AddDefaultMappingContext();

	if (HasPendingPalworldAppearance())
	{
		// 커마 완료 뒤 넘어온 게임 레벨은 로그인 화면을 다시 덮지 않고 곧바로 캐릭터를 보여준다.
		HideLoginScreen();
		return;
	}

	if (bShowLoginOnBeginPlay)
	{
		ShowLoginScreen();
	}
}

void AUEPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AUEPlayerCharacter* PlayerCharacter = Cast<AUEPlayerCharacter>(InPawn);
	UUEGameInstance* UEGameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!PlayerCharacter || !UEGameInstance)
	{
		return;
	}

	FUEPalworldAppearance PendingAppearance;
	if (UEGameInstance->GetPendingPalworldAppearance(PendingAppearance))
	{
		// 레벨 이동 직후 빙의 순서가 달라져도 저장한 커마를 다시 입힌다.
		PlayerCharacter->ApplyPalworldAppearance(PendingAppearance);
	}
}

bool AUEPlayerController::HasPendingPalworldAppearance() const
{
	FUEPalworldAppearance PendingAppearance;
	const UUEGameInstance* UEGameInstance = Cast<UUEGameInstance>(GetGameInstance());
	return UEGameInstance && UEGameInstance->GetPendingPalworldAppearance(PendingAppearance);
}

void AUEPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	BindGameplayInput();
}

void AUEPlayerController::ShowLoginScreen()
{
	if (!LoginWidgetInstance)
	{
		TSubclassOf<UUELoginWidget> WidgetClassToCreate = LoginWidgetClass;
		if (!WidgetClassToCreate)
		{
			WidgetClassToCreate = UUELoginWidget::StaticClass();
		}

		LoginWidgetInstance = CreateWidget<UUELoginWidget>(this, WidgetClassToCreate);
		if (!LoginWidgetInstance)
		{
			return;
		}
	}

	LoginWidgetInstance->OnLoginSucceeded.AddUniqueDynamic(this, &ThisClass::HandleLoginSucceeded);
	LoginWidgetInstance->AddToViewport(LoginWidgetZOrder);

	// Login UI should receive keyboard and mouse input before gameplay starts.
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(LoginWidgetInstance->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AUEPlayerController::HideLoginScreen()
{
	if (LoginWidgetInstance)
	{
		LoginWidgetInstance->OnLoginSucceeded.RemoveDynamic(this, &ThisClass::HandleLoginSucceeded);
		LoginWidgetInstance->RemoveFromParent();
	}

	// After login, return control to the character movement input path.
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
}

void AUEPlayerController::HandleLoginSucceeded(const FString& UserId, const FString& Nickname)
{
	// Gameplay input is enabled only after the local account subsystem accepted both credentials.
	HideLoginScreen();
	BP_OnLocalLoginSucceeded(UserId, Nickname);
}

void AUEPlayerController::AddDefaultMappingContext() const
{
	if (!InputData || !InputData->InputMappingContext)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
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

void AUEPlayerController::BindGameplayInput()
{
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent || !InputData)
	{
		return;
	}

	BindMoveInput(EnhancedInputComponent);
	BindLookInput(EnhancedInputComponent);
	BindActionInput(EnhancedInputComponent);
}

void AUEPlayerController::BindMoveInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	const UInputAction* MoveAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_Move);
	if (!MoveAction)
	{
		MoveAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	}

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::HandleMove);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ThisClass::HandleMoveStopped);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ThisClass::HandleMoveStopped);
	}
}

void AUEPlayerController::BindLookInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	const UInputAction* LookYawAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_LookYaw);
	const UInputAction* LookPitchAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_LookPitch);

	if (LookYawAction)
	{
		EnhancedInputComponent->BindAction(LookYawAction, ETriggerEvent::Triggered, this, &ThisClass::HandleLookYaw);
	}

	if (LookPitchAction)
	{
		EnhancedInputComponent->BindAction(LookPitchAction, ETriggerEvent::Triggered, this, &ThisClass::HandleLookPitch);
	}
}

void AUEPlayerController::BindActionInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	const UInputAction* RunAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_Run);
	if (!RunAction)
	{
		RunAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Run.IA_Run"));
	}

	if (RunAction)
	{
		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Started, this, &ThisClass::HandleRunStarted);
		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Completed, this, &ThisClass::HandleRunStopped);
		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Canceled, this, &ThisClass::HandleRunStopped);
	}

	const UInputAction* SpawnPokemonAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_SpawnPokemon);
	if (!SpawnPokemonAction)
	{
		SpawnPokemonAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_SpawnPokemon.IA_SpawnPokemon"));
	}

	if (SpawnPokemonAction)
	{
		EnhancedInputComponent->BindAction(SpawnPokemonAction, ETriggerEvent::Started, this, &ThisClass::HandlePokemonToggle);
	}
}

AUEPlayerCharacter* AUEPlayerController::GetControlledPlayerCharacter() const
{
	return Cast<AUEPlayerCharacter>(GetPawn());
}

void AUEPlayerController::HandleMove(const FInputActionValue& Value)
{
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->SetMovementInput(Value.Get<FVector2D>());
	}
}

void AUEPlayerController::HandleMoveStopped(const FInputActionValue& Value)
{
	(void)Value;

	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->SetMovementInput(FVector2D::ZeroVector);
	}
}

void AUEPlayerController::HandleLookYaw(const FInputActionValue& Value)
{
	AddYawInput(Value.Get<float>() * LookYawRate);
}

void AUEPlayerController::HandleLookPitch(const FInputActionValue& Value)
{
	AddPitchInput(-Value.Get<float>() * LookPitchRate);
}

void AUEPlayerController::HandleRunStarted(const FInputActionValue& Value)
{
	(void)Value;

	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->SetRunning(true);
	}
}

void AUEPlayerController::HandleRunStopped(const FInputActionValue& Value)
{
	(void)Value;

	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->SetRunning(false);
	}
}

void AUEPlayerController::HandlePokemonToggle(const FInputActionValue& Value)
{
	(void)Value;

	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->TogglePokemonCompanion();
	}
}
