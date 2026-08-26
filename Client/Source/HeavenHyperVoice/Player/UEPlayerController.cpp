#include "UEPlayerController.h"

#include "../Character/UEPlayerCharacter.h"
#include "../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"
#include "../Data/UEDataAsset.h"
#include "../System/UEGameInstance.h"
#include "../UEGameplayTags.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "GameFramework/CharacterMovementComponent.h"

AUEPlayerController::AUEPlayerController()
{
	bShowMouseCursor = false;
}

void AUEPlayerController::BeginPlay()
{
	Super::BeginPlay();

	AddDefaultMappingContext();
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		MaxWalkSpeed = PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed;
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

	FUEHHVAppearance PendingAppearance;
	if (UEGameInstance->GetPendingHHVAppearance(PendingAppearance))
	{
		// 레벨 이동 직후 빙의 순서가 달라져도 저장한 커마를 다시 입힌다.
		PlayerCharacter->ApplyHHVAppearance(PendingAppearance);
	}

	// 로비에서 입장한 슬롯의 스타팅 포켓몬도 같은 로컬 저장값으로 맞춘다.
	if (UUEPokemonSpeciesData* PartnerSpecies = UEGameInstance->GetSelectedPartnerSpecies())
	{
		PlayerCharacter->SetPokemonCompanionSpeciesData(PartnerSpecies);
	}
}

bool AUEPlayerController::HasPendingHHVAppearance() const
{
	FUEHHVAppearance PendingAppearance;
	const UUEGameInstance* UEGameInstance = Cast<UUEGameInstance>(GetGameInstance());
	return UEGameInstance && UEGameInstance->GetPendingHHVAppearance(PendingAppearance);
}

void AUEPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	BindGameplayInput();
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
	BindRunInput(EnhancedInputComponent);
	BindJumpInput(EnhancedInputComponent);
	BindRollInput(EnhancedInputComponent);
	BindPokemonAttackInput(EnhancedInputComponent);
}

void AUEPlayerController::BindMoveInput(UEnhancedInputComponent* EnhancedInputComponent)
{

	// 풀 받은 입력 데이터는 W/A/S/D를 하나의 2D IA_Move로 통합한다.
	const UInputAction* MoveAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_Move);
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
	const UInputAction* SpawnPokemonAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_SpawnPokemon);
	if (SpawnPokemonAction)
	{
		EnhancedInputComponent->BindAction(SpawnPokemonAction, ETriggerEvent::Started, this, &ThisClass::HandlePokemonToggle);
	}
}

void AUEPlayerController::BindRunInput(class UEnhancedInputComponent* EnhancedInputComponent)
{
	const UInputAction* RunAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_Run);
	if (!RunAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("달리는거 연결 안됐다 ㅇㅇ"));
	}
	if (RunAction)
	{
		EnhancedInputComponent -> BindAction(RunAction,ETriggerEvent::Started, this, &ThisClass::HandleRunStarted);
		EnhancedInputComponent -> BindAction(RunAction,ETriggerEvent::Completed, this, &ThisClass::HandleRunStopped);
	}
}

void AUEPlayerController::BindJumpInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	const UInputAction* JumpAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_Jump);
	if (!JumpAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("점프 연결 안됐다 ㅇㅇ"));
	}
	if (JumpAction)
	{
		EnhancedInputComponent -> BindAction(JumpAction,ETriggerEvent::Started, this, &ThisClass::HandleJump);
	}
}

void AUEPlayerController::BindRollInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	const UInputAction* RollAction = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_Roll);
	if (!RollAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("구루구루 연결 안됐다 ㅇㅇ"));
	}
	if (RollAction)
	{
		EnhancedInputComponent -> BindAction(RollAction,ETriggerEvent::Started, this, &ThisClass::HandleRoll);
	}
}

AUEPlayerCharacter* AUEPlayerController::GetControlledPlayerCharacter() const
{
	return Cast<AUEPlayerCharacter>(GetPawn());
}

void AUEPlayerController::HandleMove(const FInputActionValue& Value)
{
	// 이동 입력은 X=앞/뒤, Y=좌/우로 캐릭터까지 그대로 전달한다.
	PendingMovementInput = Value.Get<FVector2D>();
	PushMovementInputToCharacter();
}

void AUEPlayerController::HandleMoveStopped(const FInputActionValue& Value)
{
	(void)Value;
	PendingMovementInput = FVector2D::ZeroVector;
	PushMovementInputToCharacter();
}

void AUEPlayerController::PushMovementInputToCharacter()
{
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->SetMovementInput(PendingMovementInput);
	}
}

void AUEPlayerController::HandleMoveForward(const FInputActionValue& Value)
{
	PendingMovementInput.X = Value.Get<float>();
	PushMovementInputToCharacter();
}

void AUEPlayerController::HandleMoveForwardStopped(const FInputActionValue& Value)
{
	if (PendingMovementInput.X > 0.0f)
	{
		PendingMovementInput.X = 0.0f;
		PushMovementInputToCharacter();
	}
}

void AUEPlayerController::HandleMoveBackward(const FInputActionValue& Value)
{
	PendingMovementInput.X = -Value.Get<float>();
	PushMovementInputToCharacter();
}

void AUEPlayerController::HandleMoveBackwardStopped(const FInputActionValue& Value)
{
	if (PendingMovementInput.X < 0.0f)
	{
		PendingMovementInput.X = 0.0f;
		PushMovementInputToCharacter();
	}
}

void AUEPlayerController::HandleMoveRight(const FInputActionValue& Value)
{
	PendingMovementInput.Y = Value.Get<float>();
	PushMovementInputToCharacter();
}

void AUEPlayerController::HandleMoveRightStopped(const FInputActionValue& Value)
{
	if (PendingMovementInput.Y > 0.0f)
	{
		PendingMovementInput.Y = 0.0f;
		PushMovementInputToCharacter();
	}
}

void AUEPlayerController::HandleMoveLeft(const FInputActionValue& Value)
{
	PendingMovementInput.Y = -Value.Get<float>();
	PushMovementInputToCharacter();
}

void AUEPlayerController::HandleMoveLeftStopped(const FInputActionValue& Value)
{
	if (PendingMovementInput.Y < 0.0f)
	{
		PendingMovementInput.Y = 0.0f;
		PushMovementInputToCharacter();
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
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->SetRunning(true);
	}
}

void AUEPlayerController::BindPokemonAttackInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	// 숫자 1~4는 임시 기술 슬롯이다. IA와 GameplayTag를 사용해 나중에 키 설정 UI에서도 교체할 수 있게 한다.
	const UInputAction* Attack1Action = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_PokemonAttack1);
	const UInputAction* Attack2Action = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_PokemonAttack2);
	const UInputAction* Attack3Action = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_PokemonAttack3);
	const UInputAction* Attack4Action = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_PokemonAttack4);

	if (Attack1Action)
	{
		EnhancedInputComponent->BindAction(Attack1Action, ETriggerEvent::Started, this, &ThisClass::HandlePokemonAttack1);
	}
	if (Attack2Action)
	{
		EnhancedInputComponent->BindAction(Attack2Action, ETriggerEvent::Started, this, &ThisClass::HandlePokemonAttack2);
	}
	if (Attack3Action)
	{
		EnhancedInputComponent->BindAction(Attack3Action, ETriggerEvent::Started, this, &ThisClass::HandlePokemonAttack3);
	}
	if (Attack4Action)
	{
		EnhancedInputComponent->BindAction(Attack4Action, ETriggerEvent::Started, this, &ThisClass::HandlePokemonAttack4);
	}
}

void AUEPlayerController::HandleRunStopped(const FInputActionValue& Value)
{
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->SetRunning(false);
	}
}

void AUEPlayerController::HandleJump(const FInputActionValue& Value)
{
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->Jump();
	}
}

void AUEPlayerController::HandleRoll(const FInputActionValue& Value)
{
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->Roll();
	}
}

void AUEPlayerController::HandlePokemonToggle(const FInputActionValue& Value)
{


	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->TogglePokemonCompanion();
	}
}

void AUEPlayerController::HandlePokemonAttack1(const FInputActionValue& Value)
{
	(void)Value;
	HandlePokemonAttackSlot(1);
}

void AUEPlayerController::HandlePokemonAttack2(const FInputActionValue& Value)
{
	(void)Value;
	HandlePokemonAttackSlot(2);
}

void AUEPlayerController::HandlePokemonAttack3(const FInputActionValue& Value)
{
	(void)Value;
	HandlePokemonAttackSlot(3);
}

void AUEPlayerController::HandlePokemonAttack4(const FInputActionValue& Value)
{
	(void)Value;
	HandlePokemonAttackSlot(4);
}

void AUEPlayerController::HandlePokemonAttackSlot(int32 AttackSlot)
{
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		// 컨트롤러는 입력 번호만 전달하고 소유 여부와 공격 가능 여부는 캐릭터와 서버 컴포넌트가 판단한다.
		PlayerCharacter->CommandPokemonAttack(AttackSlot);
	}
}
