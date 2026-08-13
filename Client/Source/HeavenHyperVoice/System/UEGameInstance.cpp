// Fill out your copyright notice in the Description page of Project Settings.


#include "UEGameInstance.h"
#include "../System/UEAssetManager.h"

UUEGameInstance::UUEGameInstance(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UUEGameInstance::Init()
{
	Super::Init();

	UUEAssetManager::Initialize();
}

void UUEGameInstance::Shutdown()
{
	Super::Shutdown();

}

void UUEGameInstance::SetPendingPalworldAppearance(const FUEPalworldAppearance& NewAppearance)
{
	PendingPalworldAppearance = NewAppearance;
	bHasPendingPalworldAppearance = true;
}

bool UUEGameInstance::GetPendingPalworldAppearance(FUEPalworldAppearance& OutAppearance) const
{
	if (!bHasPendingPalworldAppearance)
	{
		return false;
	}

	OutAppearance = PendingPalworldAppearance;
	return true;
}

void UUEGameInstance::ClearPendingPalworldAppearance()
{
	bHasPendingPalworldAppearance = false;
	PendingPalworldAppearance = FUEPalworldAppearance();
}
