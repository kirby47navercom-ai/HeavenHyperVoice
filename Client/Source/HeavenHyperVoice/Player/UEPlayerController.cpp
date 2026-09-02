#include "UEPlayerController.h"

#include "../Character/UEPlayerCharacter.h"
#include "../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"
#include "../Data/UEDataAsset.h"
#include "../Net/HHVChatConnection.h"
#include "../Server/UEFieldClientSubsystem.h"
#include "../System/UEGameInstance.h"
#include "../UI/PokemonParty/UEPokemonPartyWidget.h"
#include "../UEGameplayTags.h"

#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AUEPlayerController::AUEPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;

	static ConstructorHelpers::FClassFinder<UUserWidget> ChatWidgetFinder(
		TEXT("/Game/UI/Chat/WBP_GameChat"));
	static ConstructorHelpers::FClassFinder<UUserWidget> ChatLineFinder(
		TEXT("/Game/UI/Chat/WBP_ChatLine"));
	static ConstructorHelpers::FClassFinder<UUserWidget> ChatSystemLineFinder(
		TEXT("/Game/UI/Chat/WBP_ChatSystemLine"));
	ChatWidgetClass = ChatWidgetFinder.Class;
	ChatLineWidgetClass = ChatLineFinder.Class;
	ChatSystemLineWidgetClass = ChatSystemLineFinder.Class;
}

void AUEPlayerController::BeginPlay()
{
	Super::BeginPlay();

	AddDefaultMappingContext();
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	if (UUEFieldClientSubsystem* FieldClientSubsystem = UUEFieldClientSubsystem::Get(this))
	{
		FieldClientSubsystem->RegisterPlayerController(this);
	}
	ShowPokemonPartyWidget();
	if (IsLocalController())
	{
		CreateChatWidget();
		StartChat();
	}
	
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		MaxWalkSpeed = PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed;
	}
}

void AUEPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ChatConnection)
	{
		ChatConnection->Poll();
	}
}

void AUEPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ChatInput)
	{
		ChatInput->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleChatTextCommitted);
	}
	ChatConnection.reset();
	Super::EndPlay(EndPlayReason);
}

void AUEPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AUEPlayerCharacter* PlayerCharacter = Cast<AUEPlayerCharacter>(InPawn);
	if (!PlayerCharacter)
	{
		return;
	}

	if (UUEFieldClientSubsystem* FieldClientSubsystem = UUEFieldClientSubsystem::Get(this))
	{
		FieldClientSubsystem->RegisterPlayerController(this);
		FieldClientSubsystem->AttachPlayerCharacter(PlayerCharacter);
	}

	if (UUEGameInstance* UEGameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		FUEHHVAppearance PendingAppearance;
		if (UEGameInstance->GetPendingHHVAppearance(PendingAppearance))
		{
			// 레벨 이동 직후 빙의 순서가 달라져도 저장한 커마를 다시 입힌다.
			PlayerCharacter->ApplyHHVAppearance(PendingAppearance);
		}
	}

	// 컨트롤러보다 Pawn 빙의가 늦어도 이미 생성된 HUD에 정확한 로스터 소유자를 다시 연결한다.
	if (PokemonPartyWidget)
	{
		PokemonPartyWidget->InitializeForPlayer(PlayerCharacter);
	}
}

void AUEPlayerController::HHVEnterInstance(int32 InstanceType)
{
	if (UUEFieldClientSubsystem* FieldClientSubsystem = UUEFieldClientSubsystem::Get(this))
	{
		FieldClientSubsystem->EnterInstance(InstanceType);
	}
}

void AUEPlayerController::HHVLeaveInstance()
{
	if (UUEFieldClientSubsystem* FieldClientSubsystem = UUEFieldClientSubsystem::Get(this))
	{
		FieldClientSubsystem->LeaveInstance();
	}
}

void AUEPlayerController::ShowPokemonPartyWidget()
{
	if (!IsLocalController() || PokemonPartyWidget || !PokemonPartyWidgetClass)
	{
		return;
	}

	// 클래스는 BP_LoginPlayerController의 변수로 지정한다. 런타임 에셋 주소는 사용하지 않는다.
	PokemonPartyWidget = CreateWidget<UUEPokemonPartyWidget>(this, PokemonPartyWidgetClass);
	if (!PokemonPartyWidget)
	{
		return;
	}

	PokemonPartyWidget->AddToViewport(PokemonPartyWidgetZOrder);
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PokemonPartyWidget->InitializeForPlayer(PlayerCharacter);
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
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ThisClass::OpenChatInput);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::CloseChatInput);
}

void AUEPlayerController::CreateChatWidget()
{
	if (!IsLocalController() || ChatWidget || !ChatWidgetClass)
	{
		return;
	}

	ChatWidget = CreateWidget<UUserWidget>(this, ChatWidgetClass);
	if (!ChatWidget)
	{
		return;
	}

	ChatInput = Cast<UEditableTextBox>(ChatWidget->GetWidgetFromName(TEXT("ChatInput")));
	ChatMessageScroll = Cast<UScrollBox>(ChatWidget->GetWidgetFromName(TEXT("MessageScroll")));
	ChatMessageList = Cast<UVerticalBox>(ChatWidget->GetWidgetFromName(TEXT("MessageList")));
	if (!ChatInput || !ChatMessageScroll || !ChatMessageList)
	{
		UE_LOG(LogTemp, Error,
			TEXT("WBP_GameChat needs ChatInput, MessageScroll, and MessageList widgets"));
		ChatWidget = nullptr;
		return;
	}

	ChatInput->SetIsReadOnly(true);
	ChatInput->SetClearKeyboardFocusOnCommit(false);
	ChatInput->SetRevertTextOnEscape(false);
	ChatInput->OnTextCommitted.AddDynamic(this, &ThisClass::HandleChatTextCommitted);
	ChatWidget->AddToViewport(20);
}

void AUEPlayerController::StartChat()
{
	if (!ChatWidget || ChatConnection)
	{
		return;
	}

	const UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	FHHVChatSettings Settings;
	if (!GameInstance ||
		!GameInstance->GetChatEndpoint(Settings.Host, Settings.Port, Settings.Ticket) ||
		Settings.Ticket.IsEmpty())
	{
		AddSystemMessage(TEXT("채팅 서버 정보를 찾을 수 없습니다"));
		return;
	}

	ChatConnection = std::make_unique<FHHVChatConnection>();
	ChatConnection->OnNotice = [this](const FString& Text)
	{
		AddSystemMessage(Text);
	};
	ChatConnection->OnMessage = [this](const FString& Nickname, const FString& Text)
	{
		AddChatLine(Nickname, Text, false);
	};
	ChatConnection->OnDisconnected = [this](const FString& Reason)
	{
		if (Reason != TEXT("closed"))
		{
			AddSystemMessage(Reason);
		}
	};
	ChatConnection->Start(Settings);
}

void AUEPlayerController::OpenChatInput()
{
	if (!ChatWidget || !ChatInput || bChatInputOpen)
	{
		return;
	}

	PendingMovementInput = FVector2D::ZeroVector;
	PushMovementInputToCharacter();
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->SetRunning(false);
	}

	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(ChatInput->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bChatInputOpen = true;
	ChatInput->SetIsReadOnly(false);
	ChatInput->SetHintText(FText::FromString(TEXT("메시지를 입력하고 Enter")));
	ChatInput->SetKeyboardFocus();
}

void AUEPlayerController::CloseChatInput()
{
	if (!ChatWidget || !ChatInput || !bChatInputOpen)
	{
		return;
	}

	bChatInputOpen = false;
	ChatInput->SetText(FText::GetEmpty());
	ChatInput->SetIsReadOnly(true);
	ChatInput->SetHintText(FText::FromString(TEXT("Enter 키를 눌러 채팅")));
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	SetInputMode(FInputModeGameOnly());
}

bool AUEPlayerController::SubmitChatText(const FString& Text)
{
	if (!ChatConnection)
	{
		AddSystemMessage(TEXT("채팅 서버에 연결되어 있지 않습니다"));
		return false;
	}

	FString Error;
	if (!ChatConnection->SendSay(Text, Error))
	{
		AddSystemMessage(Error);
		return false;
	}
	return true;
}

void AUEPlayerController::HandleChatTextCommitted(
	const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}

	const FString Message = Text.ToString().TrimStartAndEnd();
	if (Message.IsEmpty() || SubmitChatText(Message))
	{
		CloseChatInput();
	}
	else if (ChatInput)
	{
		ChatInput->SetKeyboardFocus();
	}
}

void AUEPlayerController::AddSystemMessage(const FString& Text)
{
	AddChatLine(TEXT("시스템"), Text, true);
}

void AUEPlayerController::AddChatLine(
	const FString& Nickname, const FString& Text, bool bSystem)
{
	if (!ChatMessageList || Text.IsEmpty())
	{
		return;
	}

	const TSubclassOf<UUserWidget> LineClass =
		bSystem ? ChatSystemLineWidgetClass : ChatLineWidgetClass;
	if (!LineClass)
	{
		return;
	}

	while (ChatMessageCount >= MaxVisibleChatMessages &&
		ChatMessageList->GetChildrenCount() > 0)
	{
		ChatMessageList->RemoveChildAt(0);
		--ChatMessageCount;
	}

	UUserWidget* Line = CreateWidget<UUserWidget>(this, LineClass);
	UTextBlock* NameText = Line
		? Cast<UTextBlock>(Line->GetWidgetFromName(TEXT("NameText")))
		: nullptr;
	UTextBlock* BodyText = Line
		? Cast<UTextBlock>(Line->GetWidgetFromName(TEXT("BodyText")))
		: nullptr;
	if (!Line || !NameText || !BodyText)
	{
		UE_LOG(LogTemp, Error,
			TEXT("Chat line WBP needs NameText and BodyText widgets"));
		return;
	}

	NameText->SetText(FText::FromString(FString::Printf(TEXT("[%s] "), *Nickname)));
	BodyText->SetText(FText::FromString(Text));
	ChatMessageList->AddChild(Line);
	++ChatMessageCount;
	ChatMessageScroll->ScrollToEnd();
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
		PlayerCharacter->RequestPokemonToggle();
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
