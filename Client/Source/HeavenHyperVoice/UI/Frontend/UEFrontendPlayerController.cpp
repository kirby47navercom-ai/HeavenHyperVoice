#include "UEFrontendPlayerController.h"

#include "UETitleWidget.h"
#include "UECharacterNameWidget.h"
#include "../Login/UELoginWidget.h"
#include "../../CharacterSelection/UI/UECharacterSelectionWidget.h"
#include "../../System/UEGameInstance.h"

#include "Kismet/GameplayStatics.h"

void AUEFrontendPlayerController::BeginPlay()
{
	Super::BeginPlay();

	const UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (GameInstance && GameInstance->HasLocalSession())
	{
		ShowLobby();
		return;
	}

	ShowTitle();
}

void AUEFrontendPlayerController::ShowTitle()
{
	if (!TitleWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("타이틀 위젯 클래스가 BP_FrontendPlayerController 기본값에 지정되지 않았습니다."));
		return;
	}

	UUETitleWidget* TitleWidget = CreateWidget<UUETitleWidget>(this, TitleWidgetClass);
	if (!TitleWidget)
	{
		return;
	}

	TitleWidget->OnContinueRequested.AddUniqueDynamic(this, &ThisClass::HandleTitleContinueRequested);
	ReplaceCurrentWidget(TitleWidget);
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
	ReplaceCurrentWidget(LoginWidget);
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
	ReplaceCurrentWidget(NameWidget);
}

void AUEFrontendPlayerController::ReplaceCurrentWidget(UUserWidget* NewWidget)
{
	if (!NewWidget)
	{
		return;
	}

	if (CurrentWidget)
	{
		CurrentWidget->RemoveFromParent();
	}

	CurrentWidget = NewWidget;
	CurrentWidget->AddToViewport(WidgetZOrder);
	ApplyFrontendInputMode(CurrentWidget);
}

void AUEFrontendPlayerController::ApplyFrontendInputMode(UUserWidget* FocusWidget)
{
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (FocusWidget)
	{
		InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AUEFrontendPlayerController::HandleTitleContinueRequested()
{
	ShowLogin();
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
	ShowTitle();
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
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, CustomizationLevel);
}

void AUEFrontendPlayerController::HandleCharacterNameBackRequested()
{
	if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		GameInstance->ClearPendingCharacterCreation();
	}
	ShowLobby();
}
