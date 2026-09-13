#include "UEPlayerController.h"
#include "../Server/UEInstancePortal.h"
#include "../Character/UEPlayerCharacter.h"
#include "../UI/Options/UEOptionsScreensWidget.h"
#include "../Movement/UECoreMovementComponent.h"

bool AUEPlayerController::ShowPortalConfirmation(AUEInstancePortal* Portal)
{
	if (!IsValid(Portal) || !CanInteractWithWorld() || PortalConfirm || !PortalConfirmClass) return false;
	PortalConfirm = CreateWidget<UUEOptionsConfirmWidget>(this, PortalConfirmClass);
	if (!PortalConfirm) return false;
	SuspendChatInput();
	if (bMouseViewHeld) HandleMouseViewStopped(FInputActionValue());
	if (AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter())
		PlayerCharacter->GetCoreMovement()->StopMovementImmediately();
	PendingPortal = Portal;
	PortalConfirm->OnScreenActionRequested.AddUniqueDynamic(this, &ThisClass::HandlePortalAction);
	PortalConfirm->AddToViewport(120);
	PortalConfirm->SetPortalDestination(Portal->IsEntrance());
	ActivateUIWindow(PortalConfirm);
	return true;
}

void AUEPlayerController::ClosePortalConfirmation()
{
	if (PortalConfirm)
	{
		PortalConfirm->OnScreenActionRequested.RemoveDynamic(this, &ThisClass::HandlePortalAction);
		PortalConfirm->RemoveFromParent();
		PortalConfirm = nullptr;
	}
	PendingPortal.Reset();
	RefreshWindowInput();
}

void AUEPlayerController::HandlePortalAction(FName ActionId)
{
	TWeakObjectPtr<AUEInstancePortal> Portal = PendingPortal;
	ClosePortalConfirmation();
	if (ActionId == TEXT("PortalTravel") && Portal.IsValid() && !bReturningToFrontend)
		Portal->ConfirmTravel(GetControlledPlayerCharacter());
}
