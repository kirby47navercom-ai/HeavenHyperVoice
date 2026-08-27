#include "UEFieldServerBridgeComponent.h"

#include "UEFieldRemotePlayerSyncComponent.h"
#include "UEFieldWildPokemonSyncComponent.h"
#include "../Character/UEPlayerCharacter.h"
#include "../System/UEGameInstance.h"
#include "UEPlayerMovementSyncComponent.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"

UUEFieldServerBridgeComponent::UUEFieldServerBridgeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UUEFieldServerBridgeComponent::BeginDestroy()
{
	DetachFromPlayer();
	StopFieldConnection();
	Super::BeginDestroy();
}

bool UUEFieldServerBridgeComponent::IsExternalFieldServerConfigured() const
{
	return !FieldServerHost.IsEmpty() && FieldServerPort > 0;
}

bool UUEFieldServerBridgeComponent::SendPokemonToggleRequest()
{
	UE_LOG(LogTemp, Verbose,
		TEXT("FieldServerBridge: pokemon toggle request is waiting for a field protocol message."));
	return false;
}

bool UUEFieldServerBridgeComponent::SendPokemonAttackRequest(int32 AttackSlot)
{
	(void)AttackSlot;
	UE_LOG(LogTemp, Verbose,
		TEXT("FieldServerBridge: pokemon attack request is waiting for a field protocol message."));
	return false;
}

void UUEFieldServerBridgeComponent::ReplacePokemonPartyEntriesFromServer(TArray<FUEFieldPokemonPartyEntry> NewEntries)
{
	PokemonPartyEntries = MoveTemp(NewEntries);
	OnPokemonPartyChanged.Broadcast();
}

void UUEFieldServerBridgeComponent::BeginPlay()
{
	Super::BeginPlay();

	const UWorld* OwnerWorld = GetWorld();
	if (!OwnerWorld || (OwnerWorld->WorldType != EWorldType::Game && OwnerWorld->WorldType != EWorldType::PIE))
	{
		SetComponentTickEnabled(false);
		return;
	}

	SetComponentTickEnabled(FieldConnection != nullptr);
}

void UUEFieldServerBridgeComponent::AttachToPlayer(AUEPlayerCharacter* PlayerCharacter)
{
	if (!PlayerCharacter || PlayerCharacter->IsRemoteProxy())
	{
		DetachFromPlayer();
		return;
	}

	if (CachedPlayerCharacter.Get() == PlayerCharacter)
	{
		ResolveSyncComponents();
		StartFieldConnection();
		return;
	}

	DetachFromPlayer();
	CachedPlayerCharacter = PlayerCharacter;
	ResolveSyncComponents();
	PlayerCharacter->OnCharacterMovementUpdated.AddDynamic(
		this,
		&ThisClass::HandleCharacterMovementUpdated);
	StartFieldConnection();
}

void UUEFieldServerBridgeComponent::DetachFromPlayer()
{
	if (AUEPlayerCharacter* PlayerCharacter = CachedPlayerCharacter.Get())
	{
		PlayerCharacter->OnCharacterMovementUpdated.RemoveDynamic(
			this,
			&ThisClass::HandleCharacterMovementUpdated);
	}

	StopFieldConnection();
	DestroyPresentationActors();

	CachedPlayerCharacter.Reset();
	MovementSyncComponent.Reset();
	WildPokemonSyncComponent.Reset();
	RemotePlayerSyncComponent.Reset();
	TimeSinceLastSend = 0.0f;
}

void UUEFieldServerBridgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DetachFromPlayer();
	Super::EndPlay(EndPlayReason);
}

void UUEFieldServerBridgeComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (FieldConnection)
	{
		FieldConnection->Poll();
	}
}

void UUEFieldServerBridgeComponent::ResolveSyncComponents()
{
	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return;
	}

	MovementSyncComponent = PlayerCharacter->GetMovementSyncComponent();
	WildPokemonSyncComponent = PlayerCharacter->FindComponentByClass<UUEFieldWildPokemonSyncComponent>();
	RemotePlayerSyncComponent = PlayerCharacter->FindComponentByClass<UUEFieldRemotePlayerSyncComponent>();

	if (WildPokemonSyncComponent.IsValid())
	{
		WildPokemonSyncComponent->SetWildPokemonClass(WildPokemonClass);
	}
}

void UUEFieldServerBridgeComponent::StartFieldConnection()
{
	if (FieldConnection)
	{
		return;
	}

	const UWorld* OwnerWorld = GetWorld();
	if (!OwnerWorld || (OwnerWorld->WorldType != EWorldType::Game && OwnerWorld->WorldType != EWorldType::PIE))
	{
		return;
	}

	if (!GetPlayerCharacter())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("FieldServerBridge: no attached local player; field connection is delayed."));
		return;
	}

	if (FieldServerHost.IsEmpty() || FieldServerPort <= 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("FieldServerBridge: FieldServerHost or FieldServerPort is not configured."));
		return;
	}

	ResolveSyncComponents();
	if (!MovementSyncComponent.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("FieldServerBridge: PlayerMovementSyncComponent is missing; local player corrections are disabled."));
	}
	if (!WildPokemonSyncComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("FieldServerBridge: FieldWildPokemonSyncComponent is missing; wild entities will be ignored."));
	}
	if (!RemotePlayerSyncComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("FieldServerBridge: FieldRemotePlayerSyncComponent is missing; remote players will be ignored."));
	}

	FieldConnection = std::make_unique<FHHVFieldConnection>();
	FieldConnection->OnEnterAck = [this](uint64 EntityId, float ServerX, float ServerY, float Facing)
	{
		HandleFieldEnterAck(EntityId, ServerX, ServerY, Facing);
	};
	FieldConnection->OnCorrection = [this](uint32 Sequence, float ServerX, float ServerY, float Facing)
	{
		HandleFieldCorrection(Sequence, ServerX, ServerY, Facing);
	};
	FieldConnection->OnSnapshot = [this](const FHHVFieldSnapshot& Snapshot)
	{
		HandleFieldSnapshot(Snapshot);
	};
	FieldConnection->OnDisconnected = [this](const FString& Reason)
	{
		HandleFieldDisconnected(Reason);
	};

	uint64 ResolvedCharacterId = static_cast<uint64>(DevCharacterId);
	int32 CharIdOverride = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("DevCharId="), CharIdOverride) && CharIdOverride > 0)
	{
		ResolvedCharacterId = static_cast<uint64>(CharIdOverride);
	}
	else if (DevCharacterId == DefaultDevCharacterId)
	{
		const uint32 MachineHash = FCrc::StrCrc32(*FPlatformMisc::GetLoginId());
		ResolvedCharacterId = 9000 + static_cast<uint64>(MachineHash % 100000);
	}

	FString ResolvedName = DevCharacterName;
	FString NameOverride;
	if (FParse::Value(FCommandLine::Get(), TEXT("DevName="), NameOverride) && !NameOverride.IsEmpty())
	{
		ResolvedName = NameOverride;
	}
	else if (ResolvedName == TEXT("UEClient"))
	{
		ResolvedName = FString::Printf(TEXT("UEClient-%llu"), ResolvedCharacterId);
	}

	FHHVFieldSettings Settings;
	Settings.Host = FieldServerHost;
	Settings.Port = FieldServerPort;
	Settings.DevName = ResolvedName;
	Settings.DevCharacterId = ResolvedCharacterId;

	if (const UWorld* World = GetWorld())
	{
		if (const UUEGameInstance* GameInstance = Cast<UUEGameInstance>(World->GetGameInstance()))
		{
			FString TicketHost;
			int32 TicketPort = 0;
			TArray<uint8> Ticket;
			if (GameInstance->GetFieldEndpoint(TicketHost, TicketPort, Ticket) && Ticket.Num() > 0)
			{
				Settings.Host = TicketHost;
				Settings.Port = TicketPort;
				Settings.Ticket = MoveTemp(Ticket);
			}
		}
	}

	FieldConnection->Start(Settings);

	UE_LOG(LogTemp, Display, TEXT("FieldServerBridge: connecting to %s:%d as %s (id %llu)"),
		*Settings.Host, Settings.Port, *Settings.DevName, Settings.DevCharacterId);

	SetComponentTickEnabled(true);
}

void UUEFieldServerBridgeComponent::StopFieldConnection()
{
	FieldConnection.reset();
	SetComponentTickEnabled(false);
}

void UUEFieldServerBridgeComponent::DestroyPresentationActors()
{
	if (WildPokemonSyncComponent.IsValid())
	{
		WildPokemonSyncComponent->DestroyWildPokemons();
	}
	if (RemotePlayerSyncComponent.IsValid())
	{
		RemotePlayerSyncComponent->DestroyRemotePlayers();
	}
}

void UUEFieldServerBridgeComponent::HandleFieldEnterAck(
	uint64 EntityId,
	float ServerX,
	float ServerY,
	float Facing)
{
	if (!MovementSyncComponent.IsValid())
	{
		return;
	}

	MovementSyncComponent->HandleServerEnterAck(
		EntityId,
		MakeEntityLocation(ServerX, ServerY),
		FRotator(0.0f, Facing, 0.0f));
}

void UUEFieldServerBridgeComponent::HandleFieldCorrection(
	uint32 Sequence,
	float ServerX,
	float ServerY,
	float Facing)
{
	if (!MovementSyncComponent.IsValid())
	{
		return;
	}

	MovementSyncComponent->HandleServerCorrection(
		Sequence,
		FVector2D(ToUnrealAxis(ServerX), ToUnrealAxis(ServerY)),
		Facing);
}

void UUEFieldServerBridgeComponent::HandleFieldSnapshot(const FHHVFieldSnapshot& Snapshot)
{
	if (SnapshotsLogged < 20)
	{
		++SnapshotsLogged;
		UE_LOG(LogTemp, Display, TEXT("[SNAP] %d: spawned=%d moved=%d despawned=%d"),
			SnapshotsLogged,
			Snapshot.Spawned.Num(),
			Snapshot.Moved.Num(),
			Snapshot.Despawned.Num());
	}

	for (const FHHVFieldEntity& Entity : Snapshot.Spawned)
	{
		const FVector SpawnLocation = MakeEntityLocation(Entity.X, Entity.Y);
		if (Entity.Species == 0)
		{
			if (RemotePlayerSyncComponent.IsValid())
			{
				RemotePlayerSyncComponent->HandleRemotePlayerSpawned(Entity, SpawnLocation);
			}
			continue;
		}

		if (WildPokemonSyncComponent.IsValid())
		{
			WildPokemonSyncComponent->HandleWildPokemonSpawned(Entity, SpawnLocation);
		}
	}

	for (const FHHVFieldEntity& Entity : Snapshot.Moved)
	{
		const FVector TargetLocation = MakeEntityLocation(Entity.X, Entity.Y);
		if (WildPokemonSyncComponent.IsValid() && WildPokemonSyncComponent->ContainsWildPokemon(Entity.EntityId))
		{
			WildPokemonSyncComponent->HandleWildPokemonMoved(Entity, TargetLocation);
			continue;
		}

		if (RemotePlayerSyncComponent.IsValid())
		{
			RemotePlayerSyncComponent->HandleRemotePlayerMoved(Entity, TargetLocation);
		}
	}

	for (const uint64 EntityId : Snapshot.Despawned)
	{
		if (WildPokemonSyncComponent.IsValid() && WildPokemonSyncComponent->HandleWildPokemonDespawned(EntityId))
		{
			continue;
		}
		if (RemotePlayerSyncComponent.IsValid())
		{
			RemotePlayerSyncComponent->HandleRemotePlayerDespawned(EntityId);
		}
	}
}

void UUEFieldServerBridgeComponent::HandleFieldDisconnected(const FString& Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("FieldServerBridge: disconnected: %s"), *Reason);
}

FVector UUEFieldServerBridgeComponent::MakeEntityLocation(float ServerX, float ServerY) const
{
	const AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	const double GroundZ = PlayerCharacter ? PlayerCharacter->GetActorLocation().Z : 0.0;
	return FVector(ToUnrealAxis(ServerX), ToUnrealAxis(ServerY), GroundZ);
}

AUEPlayerCharacter* UUEFieldServerBridgeComponent::GetPlayerCharacter() const
{
	return CachedPlayerCharacter.Get();
}

void UUEFieldServerBridgeComponent::HandleCharacterMovementUpdated(
	float DeltaSeconds,
	FVector OldLocation,
	FVector OldVelocity)
{
	(void)OldLocation;
	(void)OldVelocity;

	const AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter || !PlayerCharacter->IsLocallyControlled())
	{
		return;
	}

	if (!FieldConnection || !FieldConnection->IsInField() || !MovementSyncComponent.IsValid())
	{
		return;
	}

	TimeSinceLastSend += DeltaSeconds;
	if (TimeSinceLastSend < SendIntervalSeconds)
	{
		return;
	}
	TimeSinceLastSend = 0.0f;

	const FUEPlayerMovementPacket MovementPacket = MovementSyncComponent->CaptureMovementPacket();
	FieldConnection->SendMove(
		ToServerAxis(MovementPacket.ClientPosition.X),
		ToServerAxis(MovementPacket.ClientPosition.Y),
		static_cast<float>(MovementPacket.ActorRotation.Yaw),
		MovementPacket.Sequence);
}
