#include "UEPlayerController.h"

#include "../Character/UEPlayerCharacter.h"
#include "../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"
#include "../Data/UEDataAsset.h"
#include "../Net/HHVChatConnection.h"
#include "../Server/UEFieldClientSubsystem.h"
#include "../System/UEGameInstance.h"
#include "../UI/PokemonParty/UEPokemonPartyWidget.h"
#include "../UI/Options/UEOptionsMenuWidget.h"
#include "../UI/Options/UEOptionsScreensWidget.h"
#include "../UEGameplayTags.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "GameFramework/CharacterMovementComponent.h"

AUEPlayerController::AUEPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
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
		if (OptionsHUDClass)
		{
			OptionsHUD = CreateWidget<UUEOptionsHUDWidget>(this, OptionsHUDClass);
			if (OptionsHUD) OptionsHUD->AddToViewport(20);
		}
	}
	
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		MaxWalkSpeed = PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed;
	}
}

void AUEPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickPhotoMode(DeltaSeconds);
	if (ChatConnection)
	{
		ChatConnection->Poll();
	}
}

void AUEPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ExitPhotoMode();
	bReturningToFrontend = true;
	RemoveOptionsScreen();
	if (OptionsMenu)
	{
		OptionsMenu->OnMenuActionRequested.RemoveDynamic(this, &ThisClass::HandleOptionsAction);
		OptionsMenu->RemoveFromParent();
		OptionsMenu = nullptr;
	}
	if (OptionsHUD) OptionsHUD->RemoveFromParent();
	if (ChatInput)
	{
		ChatInput->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleChatTextCommitted);
	}
	if (ChatInputBackground)
	{
		ChatInputBackground->OnMouseButtonDownEvent.Unbind();
	}
	if (ChatDragHandle)
	{
		ChatDragHandle->OnMouseButtonDownEvent.Unbind();
		ChatDragHandle->OnMouseButtonUpEvent.Unbind();
		ChatDragHandle->OnMouseMoveEvent.Unbind();
	}
	for (int32 Index = 0; Index < ChatChannelTabs.Num(); ++Index)
	{
		if (!ChatChannelTabs[Index])
		{
			continue;
		}
		ChatChannelTabs[Index]->OnMouseButtonDownEvent.Unbind();
	}
	ChatConnection.reset();
	UIWindowOrder.Empty();
	Super::EndPlay(EndPlayReason);
}

void AUEPlayerController::OnPossess(APawn* InPawn)
{
	ExitPhotoMode();
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
	if (bPhotoMode) return;
	if (UUEFieldClientSubsystem* FieldClientSubsystem = UUEFieldClientSubsystem::Get(this))
	{
		FieldClientSubsystem->EnterInstance(InstanceType);
	}
}

void AUEPlayerController::HHVLeaveInstance()
{
	if (bPhotoMode) return;
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
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::HandleEscape);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ThisClass::HandleGameViewportClick);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ThisClass::HandleGameViewportClick);
}

void AUEPlayerController::HandleGameViewportClick()
{
	if (bPhotoMode) return;
	// UI가 클릭을 먼저 소비하므로, 게임 뷰포트까지 도달한 클릭만 채팅 선택을 해제한다.
	// 다른 창이 열려 있다면 그 창이 입력 모드를 계속 유지한다.
	if (bReturningToFrontend || !bShowMouseCursor || bDraggingChat) return;
	CloseChatInput(false);
}

void AUEPlayerController::HandleEscape()
{
	if (bPhotoMode) { ExitPhotoMode(); return; }
	if (bChatInputOpen) CloseChatInput();
	else ToggleOptionsMenu();
}

void AUEPlayerController::ToggleOptionsMenu()
{
	if (bPhotoMode) { ExitPhotoMode(); return; }
	if (!IsLocalController() || bReturningToFrontend) return;
	if (OptionsScreen)
	{
		HandleOptionsScreenAction(TEXT("Back"));
		return;
	}
	if (OptionsMenu)
	{
		CloseOptionsMenu();
		return;
	}
	if (!OptionsMenuClass) return;
	UUEOptionsMenuWidget* Menu = CreateWidget<UUEOptionsMenuWidget>(this, OptionsMenuClass);
	if (!Menu) return;
	SuspendChatInput();
	if (bMouseViewHeld)
	{
		HandleMouseViewStopped(FInputActionValue());
	}
	PendingMovementInput = FVector2D::ZeroVector;
	PushMovementInputToCharacter();
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->SetRunning(false);
		PlayerCharacter->StopJumping();
		PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
	}
	bDraggingChat = false;
	OptionsMenu = Menu;
	Menu->OnMenuActionRequested.AddUniqueDynamic(this, &ThisClass::HandleOptionsAction);
	Menu->AddToViewport(100);
	if (OptionsHUD) OptionsHUD->SetVisibility(ESlateVisibility::Collapsed);
	ActivateUIWindow(Menu);
}

void AUEPlayerController::CloseOptionsMenu()
{
	if (!OptionsMenu || bReturningToFrontend) return;
	RemoveOptionsScreen();
	OptionsMenu->OnMenuActionRequested.RemoveDynamic(this, &ThisClass::HandleOptionsAction);
	OptionsMenu->RemoveFromParent();
	OptionsMenu = nullptr;
	if (OptionsHUD) OptionsHUD->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RefreshWindowInput();
}

void AUEPlayerController::HandleOptionsAction(FName ActionId)
{
	if (bReturningToFrontend) return;
	if (ActionId == TEXT("Resume")) CloseOptionsMenu();
	else if (ActionId == TEXT("Camera")) EnterPhotoMode();
	else if (ActionId == TEXT("Settings"))
	{
		if (OptionsMenu && !OptionsScreen && GameSettingsClass)
			ShowOptionsScreen(CreateWidget<UUEGameSettingsWidget>(this, GameSettingsClass));
	}
	else if (ActionId == TEXT("CharacterSelect") || ActionId == TEXT("Logout"))
	{
		if (!OptionsMenu || OptionsScreen || !OptionsConfirmClass) return;
		UUEOptionsConfirmWidget* Screen = CreateWidget<UUEOptionsConfirmWidget>(this, OptionsConfirmClass);
		if (!Screen) return;
		ShowOptionsScreen(Screen);
		Screen->SetAction(ActionId);
	}
}

void AUEPlayerController::ShowOptionsScreen(UUEOptionsScreenWidget* Screen)
{
	if (!Screen || !OptionsMenu) return;
	OptionsScreen = Screen;
	Screen->OnScreenActionRequested.AddUniqueDynamic(this, &ThisClass::HandleOptionsScreenAction);
	OptionsMenu->SetVisibility(ESlateVisibility::Collapsed);
	Screen->AddToViewport(110);
	ActivateUIWindow(Screen);
}

void AUEPlayerController::RemoveOptionsScreen()
{
	if (!OptionsScreen) return;
	OptionsScreen->OnScreenActionRequested.RemoveDynamic(this, &ThisClass::HandleOptionsScreenAction);
	OptionsScreen->RemoveFromParent();
	OptionsScreen = nullptr;
}

void AUEPlayerController::HandleOptionsScreenAction(FName ActionId)
{
	if (bReturningToFrontend || !OptionsScreen) return;
	if (ActionId == TEXT("Back"))
	{
		RemoveOptionsScreen();
		if (OptionsMenu)
		{
			OptionsMenu->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			ActivateUIWindow(OptionsMenu);
		}
	}
	else if (OptionsScreen->IsA<UUEOptionsConfirmWidget>()
		&& (ActionId == TEXT("CharacterSelect") || ActionId == TEXT("Logout")))
	{
		UUEGameInstance* GI = GetGameInstance<UUEGameInstance>();
		if (!GI || FrontendLevel.IsNull()) return;
		bReturningToFrontend = true;
		OptionsScreen->SetIsEnabled(false);
		if (OptionsMenu) OptionsMenu->SetIsEnabled(false);
		ChatConnection.reset();
		if (UUEFieldClientSubsystem* Field = UUEFieldClientSubsystem::Get(this)) Field->ResetForFrontend();
		GI->PrepareReturnToFrontend(ActionId == TEXT("Logout"));
		GI->OpenLevelWithLoadingScreen(FrontendLevel);
	}
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
	FEditableTextBoxStyle InputStyle = ChatInput->GetWidgetStyle();
	InputStyle.SetForegroundColor(FSlateColor(FLinearColor::White));
	InputStyle.SetReadOnlyForegroundColor(FSlateColor(FLinearColor::White));
	InputStyle.SetFocusedForegroundColor(FSlateColor(FLinearColor::White));
	ChatInput->SetWidgetStyle(InputStyle);
	ChatInput->SetForegroundColor(FLinearColor::White);
	ChatInput->SetVisibility(ESlateVisibility::HitTestInvisible);
	ChatInput->SetClearKeyboardFocusOnCommit(false);
	ChatInput->SetRevertTextOnEscape(false);
	ChatInput->OnTextCommitted.AddDynamic(this, &ThisClass::HandleChatTextCommitted);

	static const FName MovablePanelNames[] = {
		TEXT("ChatPanelSize"), TEXT("ChatLayout"), TEXT("ChatBackground")};
	for (const FName PanelName : MovablePanelNames)
	{
		UWidget* Candidate = ChatWidget->GetWidgetFromName(PanelName);
		if (Candidate && UWidgetLayoutLibrary::SlotAsCanvasSlot(Candidate))
		{
			ChatMovablePanel = Candidate;
			break;
		}
	}
	ChatDragHandle = Cast<UBorder>(ChatWidget->GetWidgetFromName(TEXT("ChannelHeader")));
	ChatInputBackground = Cast<UBorder>(ChatWidget->GetWidgetFromName(TEXT("InputBackground")));
	if (ChatDragHandle)
	{
		ChatDragHandle->OnMouseButtonDownEvent.BindDynamic(
			this, &ThisClass::HandleChatHeaderMouseButtonDown);
		ChatDragHandle->OnMouseButtonUpEvent.BindDynamic(
			this, &ThisClass::HandleChatHeaderMouseButtonUp);
		ChatDragHandle->OnMouseMoveEvent.BindDynamic(
			this, &ThisClass::HandleChatHeaderMouseMove);
	}
	if (ChatInputBackground)
	{
		ChatInputBackground->OnMouseButtonDownEvent.BindDynamic(
			this, &ThisClass::HandleChatInputMouseButtonDown);
	}

	static const FName ChannelTabNames[] = {
		TEXT("ChannelTab0"), TEXT("ChannelTab1"), TEXT("ChannelTab2"), TEXT("ChannelTab3")};
	for (const FName TabName : ChannelTabNames)
	{
		ChatChannelTabs.Add(Cast<UBorder>(ChatWidget->GetWidgetFromName(TabName)));
	}
	ChatInputChannelText = Cast<UTextBlock>(ChatWidget->GetWidgetFromName(TEXT("InputChannelText")));
	if (ChatChannelTabs.Num() == 4 && ChatChannelTabs[0] && ChatChannelTabs[1])
	{
		SelectedChatTabColor = ChatChannelTabs[0]->GetBrushColor();
		NormalChatTabColor = ChatChannelTabs[1]->GetBrushColor();
		ChatChannelTabs[0]->OnMouseButtonDownEvent.BindDynamic(
			this, &ThisClass::HandleChannelTab0MouseButtonDown);
		ChatChannelTabs[1]->OnMouseButtonDownEvent.BindDynamic(
			this, &ThisClass::HandleChannelTab1MouseButtonDown);
		if (ChatChannelTabs[2])
		{
			ChatChannelTabs[2]->OnMouseButtonDownEvent.BindDynamic(
				this, &ThisClass::HandleChannelTab2MouseButtonDown);
		}
		if (ChatChannelTabs[3])
		{
			ChatChannelTabs[3]->OnMouseButtonDownEvent.BindDynamic(
				this, &ThisClass::HandleChannelTab3MouseButtonDown);
		}
		SelectChatChannel(0);
	}
	ChatWidget->AddToViewport(5);
	ChatWidget->SetRenderOpacity(0.2f);
}

void AUEPlayerController::SelectChatChannel(int32 ChannelIndex)
{
	static const FText ChannelNames[] = {
		NSLOCTEXT("HHV", "ChatChannelAll", "전체"),
		NSLOCTEXT("HHV", "ChatChannelGeneral", "일반"),
		NSLOCTEXT("HHV", "ChatChannelParty", "파티"),
		NSLOCTEXT("HHV", "ChatChannelCombat", "전투")};
	if (!ChatChannelTabs.IsValidIndex(ChannelIndex))
	{
		return;
	}

	SelectedChatTab = ChannelIndex;

	for (int32 Index = 0; Index < ChatChannelTabs.Num(); ++Index)
	{
		if (ChatChannelTabs[Index])
		{
			ChatChannelTabs[Index]->SetBrushColor(
				Index == ChannelIndex ? SelectedChatTabColor : NormalChatTabColor);
		}
	}
	if (ChatInputChannelText)
	{
		ChatInputChannelText->SetText(ChannelNames[ChannelIndex]);
	}
	RefreshChatFilter();
}

EUEChatChannel AUEPlayerController::ChannelForSelectedTab() const
{
	switch (SelectedChatTab)
	{
	case 2:  return EUEChatChannel::Party;
	case 3:  return EUEChatChannel::Instance;
	default: return EUEChatChannel::General;  // 전체 탭에서 치면 일반으로 나간다
	}
}

void AUEPlayerController::RefreshChatFilter()
{
	if (!ChatMessageList)
	{
		return;
	}

	// 전체 탭은 필터가 아니라 "다 보여 주기" 다.
	const bool bShowAll = SelectedChatTab == 0;
	const EUEChatChannel Wanted = ChannelForSelectedTab();

	const int32 Count = ChatMessageList->GetChildrenCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UWidget* Line = ChatMessageList->GetChildAt(Index);
		if (!Line)
		{
			continue;
		}
		const EUEChatChannel LineChannel = ChatLineChannels.IsValidIndex(Index)
			? ChatLineChannels[Index]
			: EUEChatChannel::General;

		// 시스템 줄은 어느 탭에서나 보인다. 접속 끊김이나 파티 안내를 놓치면
		// 사용자는 왜 안 되는지 알 방법이 없다.
		const bool bVisible = bShowAll || LineChannel == EUEChatChannel::System ||
			LineChannel == Wanted;
		Line->SetVisibility(bVisible ? ESlateVisibility::Visible
									 : ESlateVisibility::Collapsed);
	}

	if (ChatMessageScroll)
	{
		ChatMessageScroll->ScrollToEnd();
	}
}

FEventReply AUEPlayerController::HandleChannelTab0MouseButtonDown(
	FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	SelectChatChannel(0);
	return UWidgetBlueprintLibrary::Handled();
}

FEventReply AUEPlayerController::HandleChannelTab1MouseButtonDown(
	FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	SelectChatChannel(1);
	return UWidgetBlueprintLibrary::Handled();
}

FEventReply AUEPlayerController::HandleChannelTab2MouseButtonDown(
	FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	SelectChatChannel(2);
	return UWidgetBlueprintLibrary::Handled();
}

FEventReply AUEPlayerController::HandleChannelTab3MouseButtonDown(
	FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	SelectChatChannel(3);
	return UWidgetBlueprintLibrary::Handled();
}



void AUEPlayerController::ToggleChatVisible()
{
	if (bPhotoMode || bReturningToFrontend || !ChatWidget)
	{
		return;
	}

	// 채팅을 치는 중이면 T 는 글자다. 입력칸이 포커스를 쥐고 있으면 보통
	// 여기까지 오지도 않지만, 포커스가 어긋난 순간에 창이 사라지지 않게 막는다.
	if (bChatInputOpen)
	{
		return;
	}

	bChatHidden = !bChatHidden;
	if (bChatHidden)
	{
		// 감추기 전에 입력칸을 닫는다. 열어둔 채 감추면 포커스가 보이지 않는
		// 위젯에 남아 키가 게임으로 안 간다.
		CloseChatInput();
		ChatVisibilityBeforeHide = ChatWidget->GetVisibility();
		ChatWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		// 끌 때 접혀 있었으면 접힌 채로 돌아온다. 안쪽 상태는 건드리지 않는다.
		ChatWidget->SetVisibility(ChatVisibilityBeforeHide);
	}
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
	ChatConnection->OnMessage =
		[this](const FString& Nickname, const FString& Text, EUEChatChannel Channel)
	{
		AddChatLine(Nickname, Text, false, Channel);
	};
	ChatConnection->OnDisconnected = [this](const FString& Reason)
	{
		if (Reason != TEXT("closed"))
		{
			AddSystemMessage(Reason);
		}
	};

	// 파티 명단은 GameInstance 가 들고 있는다. 이 컨트롤러는 레벨 이동에서 사라지고
	// 채팅 연결도 같이 죽지만, 파티는 그 너머까지 이어져야 한다.
	ChatConnection->OnPartyState =
		[this](uint64 PartyId, const TArray<FHHVPartyMember>& Members, const FString& Message)
	{
		if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
		{
			TArray<FUEPlayerPartyMember> Rows;
			Rows.Reserve(Members.Num());
			for (const FHHVPartyMember& Member : Members)
			{
				FUEPlayerPartyMember Row;
				Row.AccountId = static_cast<int64>(Member.AccountId);
				Row.Nickname = Member.Nickname;
				Rows.Add(MoveTemp(Row));
			}
			GameInstance->ApplyPlayerParty(static_cast<int64>(PartyId), Rows);
		}
		if (!Message.IsEmpty())
		{
			AddSystemMessage(Message);
		}
	};
	ChatConnection->OnPartyInvited = [this](uint64 PartyId, const FString& FromNickname)
	{
		PendingInvitePartyId = static_cast<int64>(PartyId);
		AddSystemMessage(FromNickname + TEXT(" 님이 파티에 초대했습니다. /수락 또는 /거절"));
		if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
		{
			GameInstance->OnPartyInvited.Broadcast(static_cast<int64>(PartyId), FromNickname);
		}
	};
	ChatConnection->OnPartyInstanceReady = [this](uint32 InstanceType)
	{
		// 파티장이 열어 준 입장이다. 파티장 본인도 여기서 넘어간다 — 포탈에서
		// 바로 가지 않고 이 신호를 기다려야 전원이 같은 경로를 탄다.
		if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
		{
			GameInstance->OnPartyInstanceReady.Broadcast(static_cast<int32>(InstanceType));
		}
		if (UUEFieldClientSubsystem* FieldClient = UUEFieldClientSubsystem::Get(this))
		{
			FieldClient->EnterInstance(static_cast<int32>(InstanceType));
		}
	};

	ChatConnection->Start(Settings);
}

void AUEPlayerController::RequestPartyInvite(const FString& TargetNickname)
{
	if (ChatConnection)
	{
		ChatConnection->SendPartyInvite(TargetNickname);
	}
}

void AUEPlayerController::RequestPartyAccept(int64 PartyId)
{
	if (ChatConnection && PartyId > 0)
	{
		ChatConnection->SendPartyAccept(static_cast<uint64>(PartyId));
	}
}

void AUEPlayerController::RequestPartyDecline(int64 PartyId)
{
	if (ChatConnection && PartyId > 0)
	{
		ChatConnection->SendPartyDecline(static_cast<uint64>(PartyId));
	}
}

void AUEPlayerController::RequestPartyLeave()
{
	if (ChatConnection)
	{
		ChatConnection->SendPartyLeave();
	}
}

void AUEPlayerController::RequestPartyKick(int64 TargetAccountId)
{
	if (ChatConnection && TargetAccountId > 0)
	{
		ChatConnection->SendPartyKick(static_cast<uint64>(TargetAccountId));
	}
}

void AUEPlayerController::RequestPartyEnterInstance(int32 InstanceType)
{
	if (ChatConnection && InstanceType > 0)
	{
		ChatConnection->SendPartyEnterInstance(static_cast<uint32>(InstanceType));
	}
}

void AUEPlayerController::OpenChatInput()
{
	if (bPhotoMode || bReturningToFrontend || !ChatWidget || !ChatInput) return;
	bChatHidden = false;
	ChatWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	bChatInputOpen = true;
	// 편집 상자에 포커스를 주기 전에 커서 단축키 입력을 끝낸다.
	// 누르고 있던 키의 반복 문자가 채팅에 섞이는 것을 막기 위한 순서다.
	ChatInput->SetVisibility(bMouseViewHeld ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible);
	ChatInput->SetIsReadOnly(bMouseViewHeld);
	ChatInput->SetHintText(FText::FromString(TEXT("메시지를 입력하고 Enter")));
	ActivateUIWindow(ChatWidget, false);
	RefreshWindowInput();
}

void AUEPlayerController::CloseChatInput(bool bClearDraft)
{
	if (!ChatWidget || !ChatInput) return;
	SuspendChatInput();
	if (bClearDraft) ChatInput->SetText(FText::GetEmpty());
	DeactivateUIWindow(ChatWidget);
}

void AUEPlayerController::SuspendChatInput()
{
	bChatInputOpen = false;
	bDraggingChat = false;
	if (!ChatInput) return;
	ChatInput->SetIsReadOnly(true);
	ChatInput->SetVisibility(ESlateVisibility::HitTestInvisible);
	ChatInput->SetHintText(FText::FromString(TEXT("Enter 키를 눌러 채팅")));
}

void AUEPlayerController::ActivateUIWindow(UUserWidget* Window, bool bFocus)
{
	if (bPhotoMode) { HideGameplayUIForPhoto(); return; }
	if (bReturningToFrontend || !Window || !Window->IsInViewport() || !Window->IsVisible()) return;
	if (!UIWindowOrder.IsEmpty() && UIWindowOrder.Last().Get() == Window) return;
	if (Window != ChatWidget)
	{
		SuspendChatInput();
		UIWindowOrder.Remove(ChatWidget.Get());
	}
	UIWindowOrder.Remove(Window);
	UIWindowOrder.Add(Window);
	RefreshWindowInput(bFocus);
}

void AUEPlayerController::DeactivateUIWindow(UUserWidget* Window)
{
	if (bReturningToFrontend || !Window) return;
	const bool bWasActive = !UIWindowOrder.IsEmpty() && UIWindowOrder.Last().Get() == Window;
	UIWindowOrder.Remove(Window);
	if (Window == ChatWidget) SuspendChatInput();
	RefreshWindowInput(bWasActive);
}

void AUEPlayerController::RefreshWindowOrder()
{
	UIWindowOrder.RemoveAll([](const TWeakObjectPtr<UUserWidget>& Entry)
	{
		return !Entry.IsValid() || !Entry->IsInViewport() || !Entry->IsVisible();
	});
	const auto SetWindowOrder = [](UUserWidget* Window, int32 ZOrder)
	{
		if (UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get())
		{
			FGameViewportWidgetSlot Slot = Viewport->GetWidgetSlot(Window);
			if (!Window->IsInViewport() || Slot.ZOrder == ZOrder) return;
			// UE 5.8의 SetWidgetSlot은 내부 캔버스 순서만 바꾸므로 뷰포트 슬롯을 다시 설치한다.
			// 이때 Slate 트리는 유지해 설정값, 채팅 초안, 스크롤 위치가 초기화되지 않게 한다.
			const TSharedRef<SWidget> KeepAlive = Window->TakeWidget();
			Viewport->RemoveWidget(Window);
			Slot.ZOrder = ZOrder;
			Viewport->AddWidget(Window, Slot);
		}
	};
	for (int32 Index = 0; Index < UIWindowOrder.Num(); ++Index)
		SetWindowOrder(UIWindowOrder[Index].Get(), 100 + Index);
	if (ChatWidget)
	{
		const bool bActive = !UIWindowOrder.IsEmpty() && UIWindowOrder.Last().Get() == ChatWidget;
		ChatWidget->SetRenderOpacity(bActive ? 1.0f : 0.2f);
		if (!bActive) SetWindowOrder(ChatWidget, 5);
	}
}

void AUEPlayerController::RefreshWindowInput(bool bFocus)
{
	if (bReturningToFrontend) return;
	if (bPhotoMode) { RefreshPhotoInput(); return; }
	RefreshWindowOrder();
	UUserWidget* Active = UIWindowOrder.IsEmpty() ? nullptr : UIWindowOrder.Last().Get();
	const bool bBlockMove = Active != nullptr;
	const bool bBlockLook = bBlockMove || bMouseViewHeld;
	if (bWindowMoveBlocked != bBlockMove)
	{
		bWindowMoveBlocked = bBlockMove;
		SetIgnoreMoveInput(bBlockMove);
		PendingMovementInput = FVector2D::ZeroVector;
		PushMovementInputToCharacter();
		if (bBlockMove)
			if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
			{
				PlayerCharacter->SetRunning(false);
				PlayerCharacter->StopJumping();
			}
	}
	if (bWindowLookBlocked != bBlockLook)
	{
		bWindowLookBlocked = bBlockLook;
		SetIgnoreLookInput(bBlockLook);
	}
	bShowMouseCursor = Active != nullptr || bMouseViewHeld;
	if (Active || bMouseViewHeld)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		if (bFocus && Active)
			Mode.SetWidgetToFocus(bChatInputOpen && Active == ChatWidget && !bMouseViewHeld
				? ChatInput->TakeWidget() : Active->TakeWidget());
		SetInputMode(Mode);
		if (bFocus && !Active) UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}
	else SetInputMode(FInputModeGameOnly());
}

bool AUEPlayerController::HandleChatWindowKey(UUserWidget* Window, const FKeyEvent& Event)
{
	if (Window != ChatWidget) return false;
	if (Event.GetKey() == EKeys::Escape)
	{
		if (!Event.IsRepeat()) CloseChatInput();
		return true;
	}
	if (Event.GetKey() == EKeys::Enter && !bChatInputOpen)
	{
		if (!Event.IsRepeat()) OpenChatInput();
		return true;
	}
	return false;
}

bool AUEPlayerController::SubmitChatText(const FString& Text)
{
	if (!ChatConnection)
	{
		AddSystemMessage(TEXT("채팅 서버에 연결되어 있지 않습니다"));
		return false;
	}

	// 파티 명령은 채팅 입력줄에 얹는다. 전용 창을 만들기 전까지 이걸로 쓴다.
	// 서버가 모든 것을 다시 검사하므로 여기서는 문자열만 가른다.
	if (Text.StartsWith(TEXT("/")))
	{
		FString Command;
		FString Argument;
		if (!Text.Split(TEXT(" "), &Command, &Argument))
		{
			Command = Text;
		}
		Command = Command.ToLower();
		Argument = Argument.TrimStartAndEnd();

		if (Command == TEXT("/초대") || Command == TEXT("/invite"))
		{
			if (Argument.IsEmpty())
			{
				AddSystemMessage(TEXT("사용법: /초대 <닉네임>"));
				return false;
			}
			RequestPartyInvite(Argument);
			return true;
		}
		if (Command == TEXT("/수락") || Command == TEXT("/accept"))
		{
			if (PendingInvitePartyId == 0)
			{
				AddSystemMessage(TEXT("받은 초대가 없습니다"));
				return false;
			}
			RequestPartyAccept(PendingInvitePartyId);
			PendingInvitePartyId = 0;
			return true;
		}
		if (Command == TEXT("/거절") || Command == TEXT("/decline"))
		{
			RequestPartyDecline(PendingInvitePartyId);
			PendingInvitePartyId = 0;
			return true;
		}
		if (Command == TEXT("/탈퇴") || Command == TEXT("/leave"))
		{
			RequestPartyLeave();
			return true;
		}
		if (Command == TEXT("/파티") || Command == TEXT("/p"))
		{
			if (Argument.IsEmpty())
			{
				AddSystemMessage(TEXT("사용법: /파티 <할 말>"));
				return false;
			}
			FString PartyError;
			if (!ChatConnection->SendSayParty(Argument, PartyError))
			{
				AddSystemMessage(PartyError);
				return false;
			}
			return true;
		}

		AddSystemMessage(TEXT("모르는 명령입니다 (/초대 /수락 /거절 /탈퇴 /파티)"));
		return false;
	}

	// 고른 탭이 곧 발화 채널이다. "전체" 탭에서 치면 일반으로 나간다 —
	// 모든 채널에 동시에 말하는 것은 없다.
	FString Error;
	bool bSent = false;
	switch (ChannelForSelectedTab())
	{
	case EUEChatChannel::Party:    bSent = ChatConnection->SendSayParty(Text, Error); break;
	case EUEChatChannel::Instance: bSent = ChatConnection->SendSayInstance(Text, Error); break;
	default:                       bSent = ChatConnection->SendSay(Text, Error); break;
	}
	if (!bSent)
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
		if (bChatInputOpen && (CommitMethod == ETextCommit::OnUserMovedFocus ||
			CommitMethod == ETextCommit::OnCleared))
		{
			CloseChatInput(false);
		}
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
	// 시스템 줄은 어느 탭에서나 보인다. 접속 끊김이나 파티 안내를 놓치면
	// 사용자는 왜 안 되는지 알 방법이 없다.
	AddChatLine(TEXT("시스템"), Text, true, EUEChatChannel::System);
}

void AUEPlayerController::AddChatLine(
	const FString& Nickname, const FString& Text, bool bSystem, EUEChatChannel Channel)
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
		if (ChatLineChannels.Num() > 0)
		{
			ChatLineChannels.RemoveAt(0);
		}
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

	// 전체 탭에서는 채널이 섞이므로 파티 발화에 표시를 붙인다. 서버가 아니라
	// 여기서 붙이는 이유는, 표시 방식이 바뀌어도 프로토콜을 안 건드리려는 것이다.
	const TCHAR* Marker = TEXT("");
	if (Channel == EUEChatChannel::Party)
	{
		Marker = TEXT("[파티] ");
	}
	else if (Channel == EUEChatChannel::Instance)
	{
		Marker = TEXT("[인스턴스] ");
	}
	const FString Label = FString::Printf(TEXT("%s[%s] "), Marker, *Nickname);

	NameText->SetText(FText::FromString(Label));
	BodyText->SetText(FText::FromString(Text));

	FLinearColor NameColor = FLinearColor::White;
	if (Channel == EUEChatChannel::Party)
	{
		NameColor = FLinearColor(0.55f, 0.80f, 1.0f);
	}
	else if (Channel == EUEChatChannel::Instance)
	{
		NameColor = FLinearColor(1.0f, 0.85f, 0.50f);
	}
	NameText->SetColorAndOpacity(FSlateColor(NameColor));
	BodyText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	ChatMessageList->AddChild(Line);
	ChatLineChannels.Add(Channel);
	++ChatMessageCount;

	// 지금 탭에 안 맞으면 바로 숨긴다. 나중에 탭을 바꾸면 다시 나온다.
	const bool bVisible = SelectedChatTab == 0 || Channel == EUEChatChannel::System ||
		Channel == ChannelForSelectedTab();
	Line->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (bVisible)
	{
		ChatMessageScroll->ScrollToEnd();
	}
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
	BindMouseViewInput(EnhancedInputComponent);
	BindChatInput(EnhancedInputComponent);
	BindPhotoInput(EnhancedInputComponent);
}

void AUEPlayerController::BindChatInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (const UInputAction* Toggle = InputData->FindInputActionByTag(UEGameplayTags::Input_Action_ToggleChat))
		EnhancedInputComponent->BindAction(Toggle, ETriggerEvent::Started, this, &ThisClass::ToggleChatVisibility);
	const UInputAction* ChatAction =
		InputData->FindInputActionByTag(UEGameplayTags::Input_Action_Chat);
	if (ChatAction)
	{
		EnhancedInputComponent->BindAction(
			ChatAction, ETriggerEvent::Started, this, &ThisClass::HandleChatInputAction);
	}
}

void AUEPlayerController::HandleChatInputAction(const FInputActionValue& Value)
{
	(void)Value;
	OpenChatInput();
}

void AUEPlayerController::BindMouseViewInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	const UInputAction* MouseViewAction =
		InputData->FindInputActionByTag(UEGameplayTags::Input_Action_MouseView);
	if (!MouseViewAction)
	{
		return;
	}

	EnhancedInputComponent->BindAction(
		MouseViewAction, ETriggerEvent::Started, this, &ThisClass::HandleMouseViewStarted);
	EnhancedInputComponent->BindAction(
		MouseViewAction, ETriggerEvent::Completed, this, &ThisClass::HandleMouseViewStopped);
	EnhancedInputComponent->BindAction(
		MouseViewAction, ETriggerEvent::Canceled, this, &ThisClass::HandleMouseViewStopped);
}

void AUEPlayerController::HandleMouseViewStarted(const FInputActionValue& Value)
{
	if (bPhotoMode) return;
	if (bReturningToFrontend || bMouseViewHeld) return;
	bMouseViewHeld = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	RefreshWindowInput(false);
}

void AUEPlayerController::HandleMouseViewStopped(const FInputActionValue& Value)
{
	if (!bMouseViewHeld) return;
	bMouseViewHeld = false;
	if (bChatInputOpen && ChatInput)
	{
		ChatInput->SetVisibility(ESlateVisibility::Visible);
		ChatInput->SetIsReadOnly(false);
	}
	// 창을 선택한 동안에는 그 창을 닫을 때까지 커서 모드를 유지한다.
	// 단축키를 놓더라도 진행 중인 마우스 드래그는 계속 캡처해야 한다.
	if (!bDraggingChat) RefreshWindowInput(bChatInputOpen);
}

FEventReply AUEPlayerController::HandleChatInputMouseButtonDown(
	FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bShowMouseCursor || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::Unhandled();
	}

	OpenChatInput();
	return UWidgetBlueprintLibrary::Handled();
}

FEventReply AUEPlayerController::HandleChatHeaderMouseButtonDown(
	FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bShowMouseCursor || !ChatMovablePanel ||
		MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::Unhandled();
	}

	bDraggingChat = true;
	LastChatDragMousePosition = MouseEvent.GetScreenSpacePosition();
	FEventReply Reply = UWidgetBlueprintLibrary::Handled();
	return UWidgetBlueprintLibrary::CaptureMouse(Reply, ChatDragHandle);
}

FEventReply AUEPlayerController::HandleChatHeaderMouseButtonUp(
	FGeometry MyGeometry, const FPointerEvent& MouseEvent)

{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !bDraggingChat)
	{
		return UWidgetBlueprintLibrary::Unhandled();
	}

	bDraggingChat = false;
	FEventReply Reply = UWidgetBlueprintLibrary::Handled();
	return UWidgetBlueprintLibrary::ReleaseMouseCapture(Reply);
}

FEventReply AUEPlayerController::HandleChatHeaderMouseMove(
	FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bShowMouseCursor || !bDraggingChat || !ChatMovablePanel ||
		!MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return UWidgetBlueprintLibrary::Unhandled();
	}

	const FVector2D MousePosition = MouseEvent.GetScreenSpacePosition();
	const FVector2D Delta = MousePosition - LastChatDragMousePosition;
	LastChatDragMousePosition = MousePosition;
	if (UCanvasPanelSlot* PanelSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ChatMovablePanel))
	{
		const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), UE_SMALL_NUMBER);
		PanelSlot->SetPosition(PanelSlot->GetPosition() + Delta / ViewportScale);
	}
	return UWidgetBlueprintLibrary::Handled();
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
		PlayerCharacter->SetMovementInput(bWindowMoveBlocked || bReturningToFrontend
			? FVector2D::ZeroVector : PendingMovementInput);
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
	if (bPhotoMode && !bPhotoLookHeld) return;
	AddYawInput(Value.Get<float>() * LookYawRate);
}

void AUEPlayerController::HandleLookPitch(const FInputActionValue& Value)
{
	if (bPhotoMode && !bPhotoLookHeld) return;
	AddPitchInput(-Value.Get<float>() * LookPitchRate);
}

void AUEPlayerController::HandleRunStarted(const FInputActionValue& Value)
{
	if (bPhotoMode) return;
	if (bWindowMoveBlocked || bReturningToFrontend) return;
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
	if (bPhotoMode) return;
	if (bWindowMoveBlocked || bReturningToFrontend) return;
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->Jump();
	}
}

void AUEPlayerController::HandleRoll(const FInputActionValue& Value)
{
	if (bPhotoMode) return;
	if (bWindowMoveBlocked || bReturningToFrontend) return;
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		PlayerCharacter->Roll();
	}
}

void AUEPlayerController::HandlePokemonToggle(const FInputActionValue& Value)
{
	if (bPhotoMode) return;
	if (OptionsMenu || bChatInputOpen || bReturningToFrontend) return;


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
	if (bPhotoMode) return;
	if (bWindowMoveBlocked || bReturningToFrontend) return;
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
	{
		// 컨트롤러는 입력 번호만 전달하고 소유 여부와 공격 가능 여부는 캐릭터와 서버 연결 흐름이 판단한다.
		PlayerCharacter->CommandPokemonAttack(AttackSlot);
	}
}
