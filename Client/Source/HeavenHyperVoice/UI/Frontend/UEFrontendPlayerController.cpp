#include "UEFrontendPlayerController.h"

#include "UETitleWidget.h"
#include "UECharacterNameWidget.h"
#include "UEServerAddressWidget.h"
#include "../Login/UELoginWidget.h"
#include "../../CharacterSelection/UI/UECharacterSelectionWidget.h"
#include "../../System/UEGameInstance.h"

#include "TimerManager.h"


void AUEFrontendPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (GameInstance && GameInstance->ConsumeFrontendLoginRequest())
	{
		ShowLogin();
		return;
	}
	if (GameInstance && GameInstance->HasLocalSession())
	{
		ShowLobby();
		return;
	}

	ShowTitle();
}

void AUEFrontendPlayerController::ShowTitle()
{
	UUETitleWidget* Title = TitleLayer;
	if (Title)
	{
		// 서버 화면에서 돌아온 경우다. 배경은 그대로 두고 줌인만 한다.
		Title->SetShot(EUEFrontendShot::Title, true);
	}
	else
	{
		Title = EnsureTitleLayer(EUEFrontendShot::Title);
		if (!Title)
		{
			return;
		}
		Title->PlayIntro();
	}

	ClearCurrentWidget();
	ApplyFrontendInputMode(Title);
}

void AUEFrontendPlayerController::ShowLogin()
{
	if (!LoginWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("로그인 위젯 클래스가 BP_FrontendPlayerController 기본값에 지정되지 않았습니다."));
		return;
	}

	UUELoginWidget* LoginWidget = CreateWidget<UUELoginWidget>(this, LoginWidgetClass);
	if (!LoginWidget)
	{
		return;
	}

	LoginWidget->OnLoginSucceeded.AddUniqueDynamic(this, &ThisClass::HandleLoginSucceeded);
	LoginWidget->OnBackRequested.AddUniqueDynamic(this, &ThisClass::HandleLoginBackRequested);
	ShowOverTitleLayer(LoginWidget);
}

void AUEFrontendPlayerController::ShowServerAddress()
{
	if (!ServerAddressWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("서버 IP 위젯 클래스가 BP_FrontendPlayerController 기본값에 지정되지 않았습니다."));
		return;
	}

	UUEServerAddressWidget* ServerAddressWidget =
		CreateWidget<UUEServerAddressWidget>(this, ServerAddressWidgetClass);
	if (!ServerAddressWidget)
	{
		return;
	}

	if (const UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		ServerAddressWidget->SetInitialServerAddress(GameInstance->GetServerAddress());
	}
	ServerAddressWidget->OnServerAddressConfirmed.AddUniqueDynamic(
		this,
		&ThisClass::HandleServerAddressConfirmed);
	ServerAddressWidget->OnBackRequested.AddUniqueDynamic(
		this,
		&ThisClass::HandleServerAddressBackRequested);
	ShowOverTitleLayer(ServerAddressWidget);
}

void AUEFrontendPlayerController::ShowLobby()
{
	if (!LobbyWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("로비 위젯 클래스가 BP_FrontendPlayerController 기본값에 지정되지 않았습니다."));
		return;
	}

	UUECharacterSelectionWidget* LobbyWidget =
		CreateWidget<UUECharacterSelectionWidget>(this, LobbyWidgetClass);
	if (!LobbyWidget)
	{
		return;
	}
	LobbyWidget->OnCharacterCreationRequested.AddUniqueDynamic(
		this,
		&ThisClass::HandleCharacterCreationRequested);
	RemoveTitleLayer();
	ReplaceCurrentWidget(LobbyWidget);
}

void AUEFrontendPlayerController::ShowCharacterName()
{
	if (!CharacterNameWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("캐릭터 이름 위젯 클래스가 BP_FrontendPlayerController 기본값에 지정되지 않았습니다."));
		return;
	}

	UUECharacterNameWidget* NameWidget = CreateWidget<UUECharacterNameWidget>(this, CharacterNameWidgetClass);
	if (!NameWidget)
	{
		return;
	}
	NameWidget->OnNameConfirmed.AddUniqueDynamic(this, &ThisClass::HandleCharacterNameConfirmed);
	NameWidget->OnBackRequested.AddUniqueDynamic(this, &ThisClass::HandleCharacterNameBackRequested);
	RemoveTitleLayer();
	ReplaceCurrentWidget(NameWidget);
}

void AUEFrontendPlayerController::ReplaceCurrentWidget(UUserWidget* NewWidget, float RevealDelay)
{
	if (!NewWidget)
	{
		return;
	}

	ClearCurrentWidget();
	CurrentWidget = NewWidget;

	if (RevealDelay <= 0.0f)
	{
		CurrentWidget->AddToViewport(WidgetZOrder);
		ApplyFrontendInputMode(CurrentWidget);
		return;
	}

	// 줌아웃이 어느 정도 진행된 뒤에 패널을 붙인다. 그 사이 입력은 타이틀 겹이 받지만
	// Menu 장면이라 무시한다.
	TWeakObjectPtr<UUserWidget> PendingWidget = NewWidget;
	GetWorldTimerManager().SetTimer(RevealTimer, FTimerDelegate::CreateWeakLambda(this, [this, PendingWidget]
	{
		UUserWidget* Widget = PendingWidget.Get();
		if (Widget && CurrentWidget == Widget && !Widget->IsInViewport())
		{
			Widget->AddToViewport(WidgetZOrder);
			ApplyFrontendInputMode(Widget);
		}
	}), RevealDelay, false);
}

void AUEFrontendPlayerController::ClearCurrentWidget()
{
	GetWorldTimerManager().ClearTimer(RevealTimer);
	if (CurrentWidget)
	{
		CurrentWidget->RemoveFromParent();
		CurrentWidget = nullptr;
	}
}

UUETitleWidget* AUEFrontendPlayerController::EnsureTitleLayer(EUEFrontendShot InitialShot)
{
	if (TitleLayer)
	{
		return TitleLayer;
	}

	if (!TitleWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("타이틀 위젯 클래스가 BP_FrontendPlayerController 기본값에 지정되지 않았습니다."));
		return nullptr;
	}

	TitleLayer = CreateWidget<UUETitleWidget>(this, TitleWidgetClass);
	if (!TitleLayer)
	{
		return nullptr;
	}

	TitleLayer->OnContinueRequested.AddUniqueDynamic(this, &ThisClass::HandleTitleContinueRequested);
	// 서버·로그인 패널보다 한 칸 아래에 깐다.
	TitleLayer->AddToViewport(WidgetZOrder - 1);
	TitleLayer->SetShot(InitialShot, false);
	return TitleLayer;
}

void AUEFrontendPlayerController::RemoveTitleLayer()
{
	if (TitleLayer)
	{
		TitleLayer->OnContinueRequested.RemoveDynamic(this, &ThisClass::HandleTitleContinueRequested);
		TitleLayer->RemoveFromParent();
		TitleLayer = nullptr;
	}
}

void AUEFrontendPlayerController::ShowOverTitleLayer(UUserWidget* NewWidget)
{
	// 타이틀 장면에서 넘어올 때만 줌아웃을 보여 주고 패널을 늦게 붙인다.
	// 로그인 ↔ 서버처럼 이미 Menu 장면이면 바로 바꾼다. 타이틀 겹이 없던 경우
	// (로그인 요청을 받고 바로 로그인 화면으로 시작) 는 줌 끝 모습으로 만든다.
	const bool bZoomOut = TitleLayer
		&& TitleLayer->GetShot() == EUEFrontendShot::Title
		&& TitleLayer->HasZoomAnimation();
	if (UUETitleWidget* Title = EnsureTitleLayer(EUEFrontendShot::Menu))
	{
		Title->SetShot(EUEFrontendShot::Menu, true);
	}
	ReplaceCurrentWidget(NewWidget, bZoomOut ? MenuRevealDelay : 0.0f);
}

void AUEFrontendPlayerController::ApplyFrontendInputMode(UUserWidget* FocusWidget)
{
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (FocusWidget && FocusWidget->IsFocusable())
	{
		InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AUEFrontendPlayerController::HandleTitleContinueRequested()
{
	ShowServerAddress();
}

void AUEFrontendPlayerController::HandleServerAddressConfirmed(const FString& ServerAddress)
{
	if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		GameInstance->SetServerAddress(ServerAddress);
	}
	ShowLogin();
}

void AUEFrontendPlayerController::HandleServerAddressBackRequested()
{
	ShowTitle();
}

void AUEFrontendPlayerController::HandleLoginSucceeded(const FString& UserId, const FString& Nickname)
{
	if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		GameInstance->SetLocalSession(UserId, Nickname);
	}
	ShowLobby();
}

void AUEFrontendPlayerController::HandleLoginBackRequested()
{
	ShowServerAddress();
}

void AUEFrontendPlayerController::HandleCharacterCreationRequested(int32 SlotIndex)
{
	UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!GameInstance || GameInstance->GetSelectedCharacterSlotIndex() != SlotIndex)
	{
		return;
	}
	ShowCharacterName();
}

void AUEFrontendPlayerController::HandleCharacterNameConfirmed(const FString& CharacterName)
{
	UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!GameInstance || CustomizationLevel.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("커스터마이징 레벨이 BP_FrontendPlayerController 기본값에 지정되지 않았습니다."));
		return;
	}

	GameInstance->SetPendingCharacterName(CharacterName);
	GameInstance->OpenLevelWithLoadingScreen(CustomizationLevel);
}

void AUEFrontendPlayerController::HandleCharacterNameBackRequested()
{
	if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		GameInstance->ClearPendingCharacterCreation();
	}
	ShowLobby();
}
