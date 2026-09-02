#include "UEHHVCustomizationPlayerController.h"

#include "../Preview/UEHHVCustomizationPreviewActor.h"
#include "../UI/UEHHVCustomizationWidget.h"
#include "../../../Pokemon/UEPokemonSpeciesData.h"
#include "../../../System/UEGameInstance.h"

#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogUEHHVCustomization, Log, All);

AUEHHVCustomizationPlayerController::AUEHHVCustomizationPlayerController()
{
	PrimaryActorTick.bCanEverTick = false;
	bShowMouseCursor = true;
}

void AUEHHVCustomizationPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (APawn* DefaultPawn = GetPawn())
	{
		DefaultPawn->SetActorHiddenInGame(true);
		DefaultPawn->SetActorEnableCollision(false);
		DefaultPawn->SetActorTickEnabled(false);
	}

	for (TActorIterator<AUEHHVCustomizationPreviewActor> It(GetWorld()); It; ++It)
	{
		PreviewActor = *It;
		break;
	}

	if (PreviewActor)
	{
		SetViewTarget(PreviewActor);
	}
	else
	{
		UE_LOG(LogUEHHVCustomization, Warning, TEXT("커스터마이징 프리뷰 액터를 찾지 못했습니다."));
	}

	ShowCustomization();
}

void AUEHHVCustomizationPlayerController::ShowCustomization()
{
	if (!CustomizationWidgetClass)
	{
		UE_LOG(LogUEHHVCustomization, Error,
			TEXT("커스터마이징 위젯 클래스가 PlayerController 블루프린트 기본값에 지정되지 않았습니다."));
		return;
	}

	UUEHHVCustomizationWidget* CustomizationWidget =
		CreateWidget<UUEHHVCustomizationWidget>(this, CustomizationWidgetClass);
	if (CustomizationWidget)
	{
		CustomizationWidget->SetPreviewActor(PreviewActor);
		if (PreviewActor)
		{
			CustomizationWidget->SetCatalog(PreviewActor->GetCatalog());
		}
		CustomizationWidget->OnCustomizationConfirmed.AddUniqueDynamic(
			this,
			&ThisClass::HandleCustomizationConfirmed);
		CustomizationWidget->OnBackRequested.AddUniqueDynamic(
			this,
			&ThisClass::HandleCustomizationBackRequested);
		ReplaceCurrentWidget(CustomizationWidget);
	}
	else
	{
		UE_LOG(LogUEHHVCustomization, Error, TEXT("커스터마이징 위젯을 생성하지 못했습니다."));
	}

	ApplyInputMode(CustomizationWidget);
}

void AUEHHVCustomizationPlayerController::ShowStarterPokemon()
{
	if (!StarterPokemonWidgetClass)
	{
		UE_LOG(LogUEHHVCustomization, Error,
			TEXT("스타팅 포켓몬 위젯 클래스가 BP_HHVCustomizationPlayerController 기본값에 지정되지 않았습니다."));
		return;
	}

	UUEStarterPokemonWidget* StarterWidget =
		CreateWidget<UUEStarterPokemonWidget>(this, StarterPokemonWidgetClass);
	if (!StarterWidget)
	{
		return;
	}
	StarterWidget->OnStarterConfirmed.AddUniqueDynamic(this, &ThisClass::HandleStarterConfirmed);
	StarterWidget->OnBackRequested.AddUniqueDynamic(this, &ThisClass::HandleStarterBackRequested);
	ReplaceCurrentWidget(StarterWidget);
	ApplyInputMode(StarterWidget);
}

void AUEHHVCustomizationPlayerController::ReplaceCurrentWidget(UUserWidget* NewWidget)
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
}

void AUEHHVCustomizationPlayerController::ApplyInputMode(UUserWidget* FocusWidget)
{
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (FocusWidget)
	{
		InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AUEHHVCustomizationPlayerController::ReturnToLobby()
{
	if (LobbyLevel.IsNull())
	{
		UE_LOG(LogUEHHVCustomization, Error,
			TEXT("로비 레벨이 BP_HHVCustomizationPlayerController 기본값에 지정되지 않았습니다."));
		return;
	}
	if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		GameInstance->OpenLevelWithLoadingScreen(LobbyLevel);
	}
}

void AUEHHVCustomizationPlayerController::HandleCustomizationConfirmed()
{
	ShowStarterPokemon();
}

void AUEHHVCustomizationPlayerController::HandleCustomizationBackRequested()
{
	if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		GameInstance->ClearPendingCharacterCreation();
	}
	ReturnToLobby();
}

void AUEHHVCustomizationPlayerController::HandleStarterConfirmed(UUEPokemonSpeciesData* StarterPokemon)
{
	UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	FString CharacterName;
	FUEHHVAppearance Appearance;
	if (!GameInstance
		|| !GameInstance->GetPendingCharacterName(CharacterName)
		|| !GameInstance->GetPendingHHVAppearance(Appearance))
	{
		if (UUEStarterPokemonWidget* StarterWidget = Cast<UUEStarterPokemonWidget>(CurrentWidget))
		{
			StarterWidget->SetStatusMessage(SaveFailedMessage);
		}
		return;
	}

	// 캐릭터는 서버가 만든다. 로컬 슬롯에 저장하던 것을 대체한다 — 외형까지 함께
	// 넘어가고, 여기가 외형이 저장되는 유일한 지점이다.
	GameInstance->OnCharacterChangeCompleted.AddUniqueDynamic(
		this, &ThisClass::HandleServerCreateCompleted);
	GameInstance->RequestCreateCharacter(CharacterName, StarterPokemon, Appearance);
}

void AUEHHVCustomizationPlayerController::HandleServerCreateCompleted(bool bOk, const FString& Message)
{
	if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		GameInstance->OnCharacterChangeCompleted.RemoveDynamic(
			this, &ThisClass::HandleServerCreateCompleted);
	}

	if (!bOk)
	{
		if (UUEStarterPokemonWidget* StarterWidget = Cast<UUEStarterPokemonWidget>(CurrentWidget))
		{
			StarterWidget->SetStatusMessage(
				Message.IsEmpty() ? SaveFailedMessage : FText::FromString(Message));
		}
		return;
	}

	ReturnToLobby();
}

void AUEHHVCustomizationPlayerController::HandleStarterBackRequested()
{
	ShowCustomization();
}
