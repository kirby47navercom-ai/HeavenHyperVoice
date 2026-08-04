// Fill out your copyright notice in the Description page of Project Settings.


#include "UEGameModeBase.h"

#include "../Player/UEPlayerController.h"

AUEGameModeBase::AUEGameModeBase()
{
	PlayerControllerClass = AUEPlayerController::StaticClass();
}
