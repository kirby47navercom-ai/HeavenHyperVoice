#include "UEFieldClientSubsystem.h"

#include "../Character/UEPlayerCharacter.h"
#include "../Player/UEPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	UUEFieldClientSubsystem* GetFieldClientSubsystemFromController(const APlayerController* PlayerController)
	{
		if (!PlayerController)
		{
			return nullptr;
		}

		ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
		return LocalPlayer ? LocalPlayer->GetSubsystem<UUEFieldClientSubsystem>() : nullptr;
	}
}

UUEFieldClientSubsystem* UUEFieldClientSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	if (const APlayerController* PlayerController = Cast<APlayerController>(WorldContextObject))
	{
		return GetFieldClientSubsystemFromController(PlayerController);
	}

	if (const APawn* Pawn = Cast<APawn>(WorldContextObject))
	{
		return GetFieldClientSubsystemFromController(Cast<APlayerController>(Pawn->GetController()));
	}

	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		return GetFieldClientSubsystemFromController(World->GetFirstPlayerController());
	}

	return nullptr;
}

void UUEFieldClientSubsystem::Deinitialize()
{
	if (FieldServerBridgeComponent)
	{
		FieldServerBridgeComponent->OnPokemonPartyChanged.RemoveDynamic(
			this,
			&ThisClass::HandleBridgePokemonPartyChanged);
		FieldServerBridgeComponent->DetachFromPlayer();
		FieldServerBridgeComponent->DestroyComponent();
		FieldServerBridgeComponent = nullptr;
	}

	CachedPlayerController.Reset();
	Super::Deinitialize();
}

void UUEFieldClientSubsystem::RegisterPlayerController(AUEPlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	CachedPlayerController = PlayerController;
	EnsureFieldServerBridge();

	if (AUEPlayerCharacter* PlayerCharacter = Cast<AUEPlayerCharacter>(PlayerController->GetPawn()))
	{
		AttachPlayerCharacter(PlayerCharacter);
	}
}

void UUEFieldClientSubsystem::AttachPlayerCharacter(AUEPlayerCharacter* PlayerCharacter)
{
	if (!PlayerCharacter || PlayerCharacter->IsRemoteProxy())
	{
		return;
	}

	if (UUEFieldServerBridgeComponent* Bridge = EnsureFieldServerBridge())
	{
		Bridge->AttachToPlayer(PlayerCharacter);
	}
}

bool UUEFieldClientSubsystem::SendPokemonToggleRequest()
{
	UUEFieldServerBridgeComponent* Bridge = EnsureFieldServerBridge();
	return Bridge ? Bridge->SendPokemonToggleRequest() : false;
}

bool UUEFieldClientSubsystem::SendPokemonAttackRequest(int32 AttackSlot)
{
	UUEFieldServerBridgeComponent* Bridge = EnsureFieldServerBridge();
	return Bridge ? Bridge->SendPokemonAttackRequest(AttackSlot) : false;
}

TArray<FUEFieldPokemonPartyEntry> UUEFieldClientSubsystem::GetPokemonPartyEntries() const
{
	return FieldServerBridgeComponent
		? FieldServerBridgeComponent->GetPokemonPartyEntries()
		: TArray<FUEFieldPokemonPartyEntry>();
}

const TArray<FUEFieldPokemonPartyEntry>& UUEFieldClientSubsystem::GetCachedPokemonPartyEntries() const
{
	static const TArray<FUEFieldPokemonPartyEntry> EmptyEntries;
	return FieldServerBridgeComponent
		? FieldServerBridgeComponent->GetCachedPokemonPartyEntries()
		: EmptyEntries;
}

void UUEFieldClientSubsystem::HandleBridgePokemonPartyChanged()
{
	OnPokemonPartyChanged.Broadcast();
}

UUEFieldServerBridgeComponent* UUEFieldClientSubsystem::EnsureFieldServerBridge()
{
	if (FieldServerBridgeComponent)
	{
		return FieldServerBridgeComponent;
	}

	AUEPlayerController* PlayerController = CachedPlayerController.Get();
	if (!PlayerController)
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			PlayerController = Cast<AUEPlayerController>(LocalPlayer->GetPlayerController(GetWorld()));
			CachedPlayerController = PlayerController;
		}
	}

	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	FieldServerBridgeComponent = NewObject<UUEFieldServerBridgeComponent>(
		PlayerController,
		UUEFieldServerBridgeComponent::StaticClass(),
		TEXT("FieldServerBridgeComponent"));
	if (!FieldServerBridgeComponent)
	{
		return nullptr;
	}

	PlayerController->AddInstanceComponent(FieldServerBridgeComponent);
	FieldServerBridgeComponent->RegisterComponent();
	FieldServerBridgeComponent->OnPokemonPartyChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleBridgePokemonPartyChanged);

	return FieldServerBridgeComponent;
}
