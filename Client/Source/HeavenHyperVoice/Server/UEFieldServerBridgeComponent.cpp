#include "UEFieldServerBridgeComponent.h"

#include "UEFieldRemotePlayerSyncComponent.h"
#include "UEFieldPartnerSyncComponent.h"
#include "UEFieldPartyWidget.h"
#include "UEFieldWildPokemonSyncComponent.h"
#include "../Character/UEPlayerCharacter.h"
#include "../Pokemon/UEPokemonSpeciesData.h"
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
	PartnerSyncComponent.Reset();
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
	PartnerSyncComponent = PlayerCharacter->FindComponentByClass<UUEFieldPartnerSyncComponent>();

	if (WildPokemonSyncComponent.IsValid())
	{
		WildPokemonSyncComponent->SetWildPokemonClass(WildPokemonClass);
	}
	if (PartnerSyncComponent.IsValid())
	{
		// 파트너도 같은 액터 클래스를 쓴다. 야생이냐 파트너냐는 RenderType 으로만 갈린다.
		PartnerSyncComponent->SetPartnerPokemonClass(WildPokemonClass);
	}
}

void UUEFieldServerBridgeComponent::StartConnection(const FString& Service, uint32 InstanceType)
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
	FieldConnection->OnEnterAck = [this](uint64 EntityId, float ServerX, float ServerY,
		float Facing, uint32 RoomId, float OriginOffset)
	{
		HandleFieldEnterAck(EntityId, ServerX, ServerY, Facing, RoomId, OriginOffset);
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
	FieldConnection->OnPartyState = [this](const FHHVFieldPartyState& State)
	{
		HandleFieldPartyState(State);
	};
	FieldConnection->OnPartnerChanged = [this](uint64 EntityId, uint16 PartnerDex)
	{
		HandleFieldPartnerChanged(EntityId, PartnerDex);
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

	const bool bInstance = Service == TEXT("instance");

	FHHVFieldSettings Settings;
	// 티켓이 없는 개발 접속용 기본값. 아래에서 티켓을 찾으면 덮어쓴다.
	Settings.Host = FieldServerHost;
	Settings.Port = bInstance ? InstanceServerPort : FieldServerPort;
	Settings.DevName = ResolvedName;
	Settings.DevCharacterId = ResolvedCharacterId;
	Settings.InstanceType = InstanceType;

	if (const UWorld* World = GetWorld())
	{
		if (const UUEGameInstance* GameInstance = Cast<UUEGameInstance>(World->GetGameInstance()))
		{
			FString TicketHost;
			int32 TicketPort = 0;
			TArray<uint8> Ticket;
			// 서비스마다 티켓이 따로 서명돼 있다. 필드 티켓으로 인스턴스에
			// 들어가려 하면 서버가 audience 불일치로 거절한다.
			if (GameInstance->GetServiceEndpoint(Service, TicketHost, TicketPort, Ticket)
				&& Ticket.Num() > 0)
			{
				Settings.Host = TicketHost;
				Settings.Port = TicketPort;
				Settings.Ticket = MoveTemp(Ticket);
			}
		}
	}

	bInInstance = bInstance;
	CurrentRoomId = 0;
	FieldConnection->Start(Settings);

	UE_LOG(LogTemp, Display, TEXT("FieldServerBridge: connecting to %s %s:%d as %s (id %llu)"),
		*Service, *Settings.Host, Settings.Port, *Settings.DevName, Settings.DevCharacterId);

	SetComponentTickEnabled(true);
}

void UUEFieldServerBridgeComponent::StopFieldConnection()
{
	FieldConnection.reset();
	CurrentRoomId = 0;
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
	if (PartnerSyncComponent.IsValid())
	{
		PartnerSyncComponent->DestroyPartners();
	}
	if (PartyWidget)
	{
		PartyWidget->RemoveFromParent();
		PartyWidget = nullptr;
	}
}

void UUEFieldServerBridgeComponent::HandleFieldEnterAck(
	uint64 EntityId,
	float ServerX,
	float ServerY,
	float Facing,
	uint32 RoomId,
	float OriginOffset)
{
	LocalEntityId = EntityId;
	CurrentRoomId = RoomId;

	// 좌표 변환 기준은 서버가 알려준 값을 쓴다. 필드와 인스턴스는 월드 크기가
	// 달라 오프셋도 다른데, ini 로 양쪽을 맞추면 언젠가 어긋나고 그때는 전원이
	// 엉뚱한 자리에 서는 것으로만 드러난다. 아래 좌표 계산이 전부 이 값을 쓴다.
	//
	// 이 값보다 먼저 오는 좌표는 없다 — 서버가 EnterAck 을 Spawn 보다 앞세운다.
	if (OriginOffset != 0.0f)
	{
		WorldOriginOffset = OriginOffset;
	}

	if (!MovementSyncComponent.IsValid())
	{
		return;
	}

	MovementSyncComponent->HandleServerEnterAck(
		EntityId,
		MakeEntityLocation(ServerX, ServerY),
		FRotator(0.0f, Facing, 0.0f));

	// 내 파트너는 스냅샷에 안 온다 — 서버는 자기 자신을 자기 시야에 넣지 않는다.
	// 로그인 때 고른 캐릭터의 파트너를 그대로 쓴다.
	if (PartnerSyncComponent.IsValid())
	{
		if (const UWorld* World = GetWorld())
		{
			if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(World->GetGameInstance()))
			{
				const UUEPokemonSpeciesData* Species = GameInstance->GetSelectedPartnerSpecies();
				if (Species && Species->DexNumber > 0)
				{
					PartnerSyncComponent->AddPartner(EntityId, GetPlayerCharacter(), Species->DexNumber);
				}
			}
		}
	}
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

				// partner_species 는 spawned 에만 실린다. 여기서 안 붙이면 다시 올 기회가 없다.
				if (PartnerSyncComponent.IsValid() && Entity.PartnerSpecies > 0)
				{
					PartnerSyncComponent->AddPartner(
						Entity.EntityId,
						RemotePlayerSyncComponent->FindRemotePlayer(Entity.EntityId),
						static_cast<int32>(Entity.PartnerSpecies));
				}
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
		if (PartnerSyncComponent.IsValid())
		{
			PartnerSyncComponent->RemovePartner(EntityId);
		}
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
	UE_LOG(LogTemp, Warning, TEXT("FieldServerBridge: disconnected from %s: %s"),
		bInInstance ? TEXT("instance") : TEXT("field"), *Reason);
}

void UUEFieldServerBridgeComponent::HandleFieldPartyState(const FHHVFieldPartyState& State)
{
	PartyState.bOk = State.bOk;
	PartyState.Message = State.Message;
	PartyState.ActiveDex = static_cast<int32>(State.ActiveDex);

	PartyState.Party.Reset(State.Party.Num());
	for (const uint16 Dex : State.Party)
	{
		PartyState.Party.Add(static_cast<int32>(Dex));
	}
	PartyState.Unlocked.Reset(State.Unlocked.Num());
	for (const uint16 Dex : State.Unlocked)
	{
		PartyState.Unlocked.Add(static_cast<int32>(Dex));
	}

	// 내 파트너는 PartnerChanged 로 따로 오지 않는다 — 월드에 반영된 결과가
	// 여기 실려 오므로 이걸로 바꾼다.
	HandleFieldPartnerChanged(LocalEntityId, State.ActiveDex);

	OnPartyStateChanged.Broadcast();
}

void UUEFieldServerBridgeComponent::HandleFieldPartnerChanged(uint64 EntityId, uint16 PartnerDex)
{
	if (!PartnerSyncComponent.IsValid() || EntityId == 0)
	{
		return;
	}

	// 같은 액터를 새 종족으로 바꾸지 않고 지우고 다시 만든다. 메시·캡슐·
	// 애니메이션·울음이 전부 종족 데이터에 묶여 있어 교체 경로가 따로 필요하다.
	PartnerSyncComponent->RemovePartner(EntityId);
	if (PartnerDex == 0)
	{
		return;
	}

	AActor* Owner = EntityId == LocalEntityId
		? static_cast<AActor*>(GetPlayerCharacter())
		: static_cast<AActor*>(RemotePlayerSyncComponent.IsValid()
			? RemotePlayerSyncComponent->FindRemotePlayer(EntityId)
			: nullptr);
	if (Owner == nullptr)
	{
		return;  // 시야 밖이다. 다시 스폰될 때 partner_species 로 붙는다
	}

	PartnerSyncComponent->AddPartner(EntityId, Owner, static_cast<int32>(PartnerDex));
}

void UUEFieldServerBridgeComponent::TogglePartyWidget()
{
	if (PartyWidget && PartyWidget->IsInViewport())
	{
		PartyWidget->RemoveFromParent();
		PartyWidget = nullptr;
		return;
	}
	PartyWidget = nullptr;

	if (!PartyWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("FieldServerBridge: PartyWidgetClass is not assigned; set it in DefaultGame.ini."));
		return;
	}

	const AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	APlayerController* Controller =
		PlayerCharacter ? Cast<APlayerController>(PlayerCharacter->GetController()) : nullptr;
	if (!Controller)
	{
		return;
	}

	PartyWidget = CreateWidget<UUEFieldPartyWidget>(Controller, PartyWidgetClass);
	if (PartyWidget)
	{
		PartyWidget->AddToViewport();
	}
}

bool UUEFieldServerBridgeComponent::SendSetParty(const TArray<int32>& DexNumbers, int32 ActiveDex)
{
	if (!FieldConnection || !FieldConnection->IsInField())
	{
		return false;
	}

	TArray<uint16> Members;
	Members.Reserve(DexNumbers.Num());
	for (const int32 Dex : DexNumbers)
	{
		if (Dex > 0 && Dex <= MAX_uint16)
		{
			Members.Add(static_cast<uint16>(Dex));
		}
	}

	FieldConnection->SendSetParty(
		Members, ActiveDex > 0 && ActiveDex <= MAX_uint16 ? static_cast<uint16>(ActiveDex) : 0);
	return true;
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
