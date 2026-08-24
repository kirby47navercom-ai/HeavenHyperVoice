#include "UECharacterSelectionGameMode.h"

#include "UECharacterSelectionPlayerController.h"

AUECharacterSelectionGameMode::AUECharacterSelectionGameMode()
{
	PlayerControllerClass = AUECharacterSelectionPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}
