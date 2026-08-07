#include "UECustomizationGameMode.h"

#include "UECustomizationPlayerController.h"

AUECustomizationGameMode::AUECustomizationGameMode()
{
	PlayerControllerClass = AUECustomizationPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}
