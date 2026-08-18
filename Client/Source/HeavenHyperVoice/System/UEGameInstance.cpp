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

void UUEGameInstance::SetPendingHHVAppearance(const FUEHHVAppearance& NewAppearance)
{
	PendingHHVAppearance = NewAppearance;
	bHasPendingHHVAppearance = true;
}

bool UUEGameInstance::GetPendingHHVAppearance(FUEHHVAppearance& OutAppearance) const
{
	if (!bHasPendingHHVAppearance)
	{
		return false;
	}

	OutAppearance = PendingHHVAppearance;
	return true;
}

void UUEGameInstance::ClearPendingHHVAppearance()
{
	bHasPendingHHVAppearance = false;
	PendingHHVAppearance = FUEHHVAppearance();
}
