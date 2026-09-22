#include "UEFieldServerBridgeComponent.h"
#include "../Data/UEProjectAssets.h"

#include "UEFieldRemotePlayerSyncComponent.h"
#include "UEFieldPartnerSyncComponent.h"
#include "UEFieldPartyWidget.h"
#include "UEFieldWildPokemonSyncComponent.h"
#include "../Character/UEPlayerCharacter.h"
#include "../Pokemon/UEPokemonSpeciesCatalog.h"
#include "../Pokemon/UEPokemonSpeciesData.h"
#include "../Pokemon/UEPokemonCharacter.h"
#include "../System/UEGameInstance.h"

#include "Engine/World.h"
#include "../Movement/UECoreMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "YANG2_CLIENT_AUTHORITY_ONLY bridge changes must never compile on main."
#endif

namespace
{
	// YANG2_CLIENT_AUTHORITY_ONLY: 실제 서버 room_id와 헷갈리지 않는 로컬 표식.
	constexpr uint32 Yang2LocalRoomPrefix = 0x59000000u;
	constexpr double Yang2WeatherPublishSeconds = 1.0;
}

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
	// YANG2_CLIENT_AUTHORITY_ONLY: 이 브랜치는 외부 필드 서버를 사용하지 않는다.
	return false;
}

bool UUEFieldServerBridgeComponent::SendPokemonAttackRequest(int32 AttackSlot)
{
	// YANG2_CLIENT_AUTHORITY_ONLY: 현재 서버 프로토콜에도 플레이어 공격 요청은 아직
	// 없으므로, 로컬 파트너의 공격 모션을 실행해 입력과 연출을 시험한다.
	return bYang2ClientAuthorityActive && AttackSlot >= 1 && AttackSlot <= 4 &&
		PartyState.ActiveDex > 0 && PartnerSyncComponent.IsValid() &&
		PartnerSyncComponent->PlayLocalPartnerAttack(LocalEntityId, ++Yang2LocalAttackSequence);
}

void UUEFieldServerBridgeComponent::ReplacePokemonPartyEntriesFromServer(
    TArray<FUEFieldPokemonPartyEntry> NewEntries)
{
	PokemonPartyEntries = MoveTemp(NewEntries);
	OnPokemonPartyChanged.Broadcast();
}

void UUEFieldServerBridgeComponent::BeginPlay()
{
	Super::BeginPlay();

	const UWorld *OwnerWorld = GetWorld();
	if (!OwnerWorld ||
	    (OwnerWorld->WorldType != EWorldType::Game && OwnerWorld->WorldType != EWorldType::PIE))
	{
		SetComponentTickEnabled(false);
		return;
	}

	SetComponentTickEnabled(FieldConnection != nullptr);
}

void UUEFieldServerBridgeComponent::AttachToPlayer(AUEPlayerCharacter *PlayerCharacter)
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
	PlayerCharacter->GetCoreMovement()->OnMovementUpdated.AddDynamic(
	    this, &ThisClass::HandleCharacterMovementUpdated);
	StartFieldConnection();
}

void UUEFieldServerBridgeComponent::DetachFromPlayer()
{
	if (AUEPlayerCharacter *PlayerCharacter = CachedPlayerCharacter.Get())
	{
		PlayerCharacter->GetCoreMovement()->OnMovementUpdated.RemoveDynamic(
		    this, &ThisClass::HandleCharacterMovementUpdated);
	}

	StopFieldConnection();
	DestroyPresentationActors();

	CachedPlayerCharacter.Reset();
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

void UUEFieldServerBridgeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (FieldConnection)
	{
		FieldConnection->Poll();
	}

	TickYang2ClientAuthority(DeltaTime);
}

void UUEFieldServerBridgeComponent::ResolveSyncComponents()
{
	AUEPlayerCharacter *PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return;
	}

	WildPokemonSyncComponent = PlayerCharacter->FindComponentByClass<UUEFieldWildPokemonSyncComponent>();
	RemotePlayerSyncComponent = PlayerCharacter->FindComponentByClass<UUEFieldRemotePlayerSyncComponent>();
	PartnerSyncComponent = PlayerCharacter->FindComponentByClass<UUEFieldPartnerSyncComponent>();

	if (!WildPokemonClass)
	{
		if (const UUEProjectAssets* Assets = UUEProjectAssetSettings::GetProjectAssets())
			WildPokemonClass = Assets->PokemonClass.LoadSynchronous();
	}

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

void UUEFieldServerBridgeComponent::StartConnection(const FString &Service, uint32 InstanceType)
{
	// YANG2_CLIENT_AUTHORITY_ONLY
	// main에서는 아래 조기 반환 전체가 없어야 한다. 네트워크 연결 대신 동일한
	// 날씨 계산을 클라이언트 프로세스에서 실행한다.
	if (bYang2ClientAuthorityActive)
	{
		return;
	}
	StartYang2ClientAuthority(InstanceType);
	return;

	// 아래는 main의 서버 권위 연결 코드다. Yang2에서는 위에서 반드시 반환한다.
	if (FieldConnection)
	{
		return;
	}
	const UWorld *OwnerWorld = GetWorld();
	if (!OwnerWorld ||
	    (OwnerWorld->WorldType != EWorldType::Game && OwnerWorld->WorldType != EWorldType::PIE))
	{
		return;
	}

	if (!GetPlayerCharacter())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("FieldServerBridge: no attached local player; field connection is delayed."));
		return;
	}

	GetPlayerCharacter()->GetCoreMovement()->PrepareForNetworkSimulation(ShouldAllowOfflineMovement());

	if (FieldServerHost.IsEmpty() || FieldServerPort <= 0)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("FieldServerBridge: FieldServerHost or FieldServerPort is not configured."));
		return;
	}

	ResolveSyncComponents();
	if (!WildPokemonSyncComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("FieldServerBridge: FieldWildPokemonSyncComponent is missing; wild entities will be "
		            "ignored."));
	}
	if (!RemotePlayerSyncComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("FieldServerBridge: FieldRemotePlayerSyncComponent is missing; remote players will be "
		            "ignored."));
	}

	FieldConnection = std::make_unique<FHHVFieldConnection>();
	FieldConnection->OnEnterAck = [this](const FHHVFieldEventData &Event) { HandleFieldEnterAck(Event); };
	FieldConnection->OnCorrection = [this](uint32 Sequence, const hhv::movement::State &State,
                                         uint64 DiscardedInputs, uint64 InputEpoch) {
		GetPlayerCharacter()->GetCoreMovement()->AcknowledgeNetworkInput(Sequence, State, DiscardedInputs, InputEpoch);
	};
	FieldConnection->OnSnapshot = [this](const FHHVFieldSnapshot &Snapshot) {
		HandleFieldSnapshot(Snapshot);
	};
	FieldConnection->OnDisconnected = [this](const FString &Reason) { HandleFieldDisconnected(Reason); };
	FieldConnection->OnPartyState = [this](const FHHVFieldPartyState &State) {
		HandleFieldPartyState(State);
	};
	FieldConnection->OnPartnerChanged = [this](uint64 EntityId, uint16 PartnerDex) {
		HandleFieldPartnerChanged(EntityId, PartnerDex);
	};
	FieldConnection->OnGachaResult = [this](const FHHVFieldGachaResult& Result)
	{
		HandleFieldGachaResult(Result);
	};
	FieldConnection->OnTokenBalance = [this](uint32 Tokens)
	{
		HandleFieldTokenBalance(Tokens);
	};
	FieldConnection->OnWeatherState = [this](const FHHVInstanceWeatherState &Weather)
	{
		HandleInstanceWeatherState(Weather);
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

	if (const UWorld *World = GetWorld())
	{
		if (const UUEGameInstance *GameInstance = Cast<UUEGameInstance>(World->GetGameInstance()))
		{
			FString TicketHost;
			int32 TicketPort = 0;
			TArray<uint8> Ticket;
			// 서비스마다 티켓이 따로 서명돼 있다. 필드 티켓으로 인스턴스에
			// 들어가려 하면 서버가 audience 불일치로 거절한다.
			if (GameInstance->GetServiceEndpoint(Service, TicketHost, TicketPort, Ticket) && Ticket.Num() > 0)
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

	UE_LOG(LogTemp, Display, TEXT("FieldServerBridge: connecting to %s %s:%d as %s (id %llu)"), *Service,
	       *Settings.Host, Settings.Port, *Settings.DevName, Settings.DevCharacterId);

	SetComponentTickEnabled(true);
}

void UUEFieldServerBridgeComponent::StopFieldConnection()
{
	FieldConnection.reset();
	Yang2LocalWeather.reset();
	bYang2ClientAuthorityActive = false;
	Yang2WeatherAccumulator = 0.0;
	Yang2LocalAttackSequence = 0;
	bInInstance = false;
	LocalEntityId = 0;
	CurrentRoomId = 0;
	bHasInstanceWeatherState = false;
	InstanceWeatherState = {};
	SetComponentTickEnabled(false);
}

void UUEFieldServerBridgeComponent::StartYang2ClientAuthority(uint32 InstanceType)
{
	AUEPlayerCharacter *Player = GetPlayerCharacter();
	if (!Player)
	{
		return;
	}

	// 서버 EnterAck를 기다리지 않고 공통 이동 코어가 로컬 상태를 권위 상태로 쓴다.
	Player->GetCoreMovement()->PrepareForNetworkSimulation(/*bAllowLocalSimulation=*/true);

	bYang2ClientAuthorityActive = true;
	LocalEntityId = 1;
	bInInstance = InstanceType > 0;
	CurrentRoomId = bInInstance ? Yang2LocalRoomPrefix | (InstanceType & 0x00FFFFFFu) : 0;
	Yang2WeatherAccumulator = 0.0;
	bHasInstanceWeatherState = false;
	InstanceWeatherState = {};

	if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
	{
		GameInstance->PrepareYang2GameplayState();
		PokemonTokens = GameInstance->GetYang2PokemonTokens();
		RefreshYang2PartyState(true, TEXT("Yang2 로컬 게임 상태"));
		OnTokenBalanceChanged.Broadcast(PokemonTokens);
	}

	if (bInInstance)
	{
		Yang2LocalWeather = std::make_unique<heaven::instance::InstanceWeather>();
		const heaven::instance::InstanceWeatherProfile Profile;
		Yang2LocalWeather->initialize(InstanceType, CurrentRoomId, Profile);
		PublishYang2ClientWeather();
	}

	UE_LOG(LogTemp, Warning,
	       TEXT("YANG2 CLIENT AUTHORITY: server connection skipped; local solo gameplay is active "
	            "(instance type %u, local room %u). DO NOT MERGE THIS PATH TO MAIN."),
	       InstanceType, CurrentRoomId);

	// 인스턴스 날씨는 초당 한 번 진행한다. 필드에서도 브릿지를 살려 두면 목적지
	// 전환 디버깅 로그를 그대로 볼 수 있어서 로컬 모드 동안은 틱을 유지한다.
	SetComponentTickEnabled(true);
}

void UUEFieldServerBridgeComponent::RefreshYang2PartyState(bool bOk, const FString& Message)
{
	// YANG2_CLIENT_AUTHORITY_ONLY
	UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		return;
	}
	PartyState.Party = GameInstance->GetYang2PokemonParty();
	PartyState.ActiveDex = GameInstance->GetYang2ActivePokemon();
	PartyState.Unlocked = GameInstance->GetYang2UnlockedPokemon();
	PartyState.bOk = bOk;
	PartyState.Message = Message;

	// HUD가 쓰는 상세 파티 목록도 같은 로컬 상태에서 만든다. 서버 패킷이 없더라도
	// 데이터 에셋에 지정된 이름과 아이콘이 그대로 표시된다.
	TArray<FUEFieldPokemonPartyEntry> LocalEntries;
	if (UUEPokemonSpeciesCatalog* Catalog = GameInstance->GetPartySpeciesCatalog())
	{
		LocalEntries.Reserve(PartyState.Party.Num());
		for (const int32 DexNumber : PartyState.Party)
		{
			UUEPokemonSpeciesData* Species = Catalog->FindByDex(DexNumber);
			if (!Species)
			{
				continue;
			}

			FUEFieldPokemonPartyEntry& Entry = LocalEntries.AddDefaulted_GetRef();
			Entry.PokemonInstanceId = DexNumber;
			Entry.SpeciesId = Species->SpeciesId;
			Entry.DisplayName = Species->DisplayName.IsEmpty()
				? FText::FromName(Species->SpeciesId)
				: Species->DisplayName;
			Entry.ProfileIcon = Species->ProfileIcon;
			Entry.Level = 1;
			Entry.CurrentHP = 100.0f;
			Entry.MaxHP = 100.0f;
			Entry.bSelected = DexNumber == PartyState.ActiveDex;
			Entry.bCanSummon = true;
		}
	}
	ReplacePokemonPartyEntriesFromServer(MoveTemp(LocalEntries));

	RefreshYang2LocalPartner();
	OnPartyStateChanged.Broadcast();
}

void UUEFieldServerBridgeComponent::RefreshYang2LocalPartner()
{
	// YANG2_CLIENT_AUTHORITY_ONLY
	if (!PartnerSyncComponent.IsValid() || LocalEntityId == 0)
	{
		return;
	}
	PartnerSyncComponent->RemovePartner(LocalEntityId);
	if (PartyState.ActiveDex > 0)
	{
		PartnerSyncComponent->AddLocalPartner(LocalEntityId, GetPlayerCharacter(), PartyState.ActiveDex);
	}
}

void UUEFieldServerBridgeComponent::TickYang2ClientAuthority(float DeltaTime)
{
	if (!bYang2ClientAuthorityActive || !Yang2LocalWeather || !bInInstance)
	{
		return;
	}

	Yang2WeatherAccumulator += FMath::Max(0.0, static_cast<double>(DeltaTime));
	if (Yang2WeatherAccumulator < Yang2WeatherPublishSeconds)
	{
		return;
	}

	const double Elapsed = Yang2WeatherAccumulator;
	Yang2WeatherAccumulator = 0.0;
	Yang2LocalWeather->advance(Elapsed);
	PublishYang2ClientWeather();
}

void UUEFieldServerBridgeComponent::PublishYang2ClientWeather()
{
	if (!Yang2LocalWeather)
	{
		return;
	}

	const heaven::instance::InstanceWeatherSnapshot Source = Yang2LocalWeather->snapshot();
	FHHVInstanceWeatherState Weather;
	Weather.RoomId = Source.roomId;
	Weather.Revision = Source.revision;
	Weather.SimulationTimeSeconds = Source.simulationTimeSeconds;
	Weather.TemperatureC = Source.temperatureC;
	Weather.RelativeHumidityPct = Source.relativeHumidityPct;
	Weather.PressureHpa = Source.pressureHpa;
	Weather.CloudCover = Source.cloudCover;
	Weather.PrecipitationMmPerHour = Source.precipitationMmPerHour;
	Weather.WindSpeedMps = Source.windSpeedMps;
	Weather.WindDirectionDegrees = Source.windDirectionDegrees;
	Weather.GroundWetness = Source.groundWetness;
	Weather.SnowDepthM = Source.snowDepthM;
	Weather.WaterBalanceErrorKgM2 = Source.waterBalanceErrorKgM2;

	// 서버 패킷과 같은 최종 처리 함수를 지나가므로 기존 블루프린트는 출처를
	// 구분하지 않고 OnInstanceWeatherChanged를 그대로 받을 수 있다.
	HandleInstanceWeatherState(Weather);
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

void UUEFieldServerBridgeComponent::HandleFieldEnterAck(const FHHVFieldEventData &Event)
{
	const uint64 EntityId = Event.EntityId;
	LocalEntityId = EntityId;
	CurrentRoomId = Event.RoomId;
	if (!GetPlayerCharacter()->GetCoreMovement()->BeginNetworkSimulation(Event.MovementState,
	                                                                     Event.CollisionHash))
	{
		FieldConnection->Shutdown();
		HandleFieldDisconnected(TEXT("클라이언트와 서버의 충돌 맵이 다릅니다."));
		return;
	}

	// EnterAck 에는 내 파트너 종족 정보가 없다. 종족은 로그인 때 고른 값을 쓰고,
	// 실제 위치/속도는 이후 서버 스냅샷으로 덮어쓴다.
	if (PartnerSyncComponent.IsValid())
	{
		if (const UWorld *World = GetWorld())
		{
			if (UUEGameInstance *GameInstance = Cast<UUEGameInstance>(World->GetGameInstance()))
			{
				const UUEPokemonSpeciesData *Species = GameInstance->GetSelectedPartnerSpecies();
				if (Species && Species->DexNumber > 0)
				{
					PartnerSyncComponent->AddPartner(EntityId, GetPlayerCharacter(), Species->DexNumber);
				}
			}
		}
	}
}

void UUEFieldServerBridgeComponent::HandleFieldSnapshot(const FHHVFieldSnapshot &Snapshot)
{
	const AUEPlayerCharacter* Player = GetPlayerCharacter();
	if (!Player || !Player->GetCoreMovement()->IsNetworkSimulationActive())
	{
		return;
	}

	if (SnapshotsLogged < 20)
	{
		++SnapshotsLogged;
		UE_LOG(LogTemp, Display, TEXT("[SNAP] %d: spawned=%d moved=%d despawned=%d"), SnapshotsLogged,
		       Snapshot.Spawned.Num(), Snapshot.Moved.Num(), Snapshot.Despawned.Num());
	}

	for (const FHHVFieldEntity &Entity : Snapshot.Spawned)
	{
		if (Entity.Species == 0)
		{
			if (RemotePlayerSyncComponent.IsValid())
			{
				RemotePlayerSyncComponent->HandleRemotePlayerSpawned(Entity);

				// partner_species 는 spawned 에만 실린다. 여기서 안 붙이면 다시 올 기회가 없다.
				if (PartnerSyncComponent.IsValid() && Entity.PartnerSpecies > 0)
				{
					PartnerSyncComponent->AddPartner(
					    Entity.EntityId, RemotePlayerSyncComponent->FindRemotePlayer(Entity.EntityId),
					    static_cast<int32>(Entity.PartnerSpecies));
					ApplyPartnerServerState(Entity);
				}
			}
			continue;
		}

		if (WildPokemonSyncComponent.IsValid())
		{
			WildPokemonSyncComponent->HandleWildPokemonSpawned(Entity);
		}
	}

	for (const FHHVFieldEntity &Entity : Snapshot.Moved)
	{
		ApplyPartnerServerState(Entity);
		if (WildPokemonSyncComponent.IsValid() &&
		    WildPokemonSyncComponent->ContainsWildPokemon(Entity.EntityId))
		{
			WildPokemonSyncComponent->HandleWildPokemonMoved(Entity);
			continue;
		}

		if (RemotePlayerSyncComponent.IsValid())
		{
			RemotePlayerSyncComponent->HandleRemotePlayerMoved(Entity);
		}
	}

	for (const uint64 EntityId : Snapshot.Despawned)
	{
		if (PartnerSyncComponent.IsValid())
		{
			PartnerSyncComponent->RemovePartner(EntityId);
		}
		if (WildPokemonSyncComponent.IsValid() &&
		    WildPokemonSyncComponent->HandleWildPokemonDespawned(EntityId))
		{
			continue;
		}
		if (RemotePlayerSyncComponent.IsValid())
		{
			RemotePlayerSyncComponent->HandleRemotePlayerDespawned(EntityId);
		}
	}
}

void UUEFieldServerBridgeComponent::ApplyPartnerServerState(const FHHVFieldEntity &Entity)
{
	if (!PartnerSyncComponent.IsValid() || !Entity.bHasPartnerTransform)
	{
		return;
	}

	PartnerSyncComponent->ApplyPartnerServerState(Entity.EntityId, Entity.PartnerCoreState,
	                                              Entity.bPartnerTeleported, Entity.ServerTimeSeconds);
}

bool UUEFieldServerBridgeComponent::ShouldAllowOfflineMovement() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return bAllowOfflineMovementForDevelopment;
#endif
}

void UUEFieldServerBridgeComponent::HandleFieldDisconnected(const FString &Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("FieldServerBridge: disconnected from %s: %s"),
	       bInInstance ? TEXT("instance") : TEXT("field"), *Reason);

	if (AUEPlayerCharacter* Player = GetPlayerCharacter())
	{
		Player->GetCoreMovement()->PrepareForNetworkSimulation(ShouldAllowOfflineMovement());
	}
	LocalEntityId = 0;
	CurrentRoomId = 0;
	bHasInstanceWeatherState = false;
	InstanceWeatherState = {};
	DestroyPresentationActors();
}

void UUEFieldServerBridgeComponent::HandleFieldPartyState(const FHHVFieldPartyState &State)
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

	// A repeated PartyState must not destroy an already synchronized partner.
	if (PartnerDex == 0)
	{
		PartnerSyncComponent->RemovePartner(EntityId);
		return;
	}

	AActor *Owner = EntityId == LocalEntityId
	                    ? static_cast<AActor *>(GetPlayerCharacter())
	                    : static_cast<AActor *>(RemotePlayerSyncComponent.IsValid()
	                                                ? RemotePlayerSyncComponent->FindRemotePlayer(EntityId)
	                                                : nullptr);
	if (Owner == nullptr)
	{
		return; // 시야 밖이다. 다시 스폰될 때 partner_species 로 붙는다
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
		if (const UUEProjectAssets* Assets = UUEProjectAssetSettings::GetProjectAssets())
			PartyWidgetClass = Assets->PartyWidgetClass.LoadSynchronous();
	}
	if (!PartyWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("FieldServerBridge: PartyWidgetClass is not assigned; set it in the project assets data asset."));
		return;
	}

	const AUEPlayerCharacter *PlayerCharacter = GetPlayerCharacter();
	APlayerController *Controller =
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

bool UUEFieldServerBridgeComponent::SendSetParty(const TArray<int32> &DexNumbers, int32 ActiveDex)
{
	// YANG2_CLIENT_AUTHORITY_ONLY: 서버 DB 대신 GameInstance의 로컬 시험 상태를 갱신한다.
	if (bYang2ClientAuthorityActive)
	{
		UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
		FString Message;
		const bool bOk = GameInstance &&
			GameInstance->SetYang2PokemonParty(DexNumbers, ActiveDex, Message);
		RefreshYang2PartyState(bOk, Message);
		return true;
	}

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

UUEFieldServerBridgeComponent *UUEFieldServerBridgeComponent::Find(const APlayerController *Controller)
{
	if (!Controller)
	{
		return nullptr;
	}

	if (UUEFieldServerBridgeComponent *Bridge = Controller->FindComponentByClass<UUEFieldServerBridgeComponent>())
	{
		return Bridge;
	}

	// 붙는 자리가 바뀌어도 화면이 죽지 않게 폰도 본다.
	const APawn *Pawn = Controller->GetPawn();
	return Pawn ? Pawn->FindComponentByClass<UUEFieldServerBridgeComponent>() : nullptr;
}

bool UUEFieldServerBridgeComponent::SendGachaDraw(EUEGachaType Type)
{
	// YANG2_CLIENT_AUTHORITY_ONLY: 데이터 에셋의 같은 가중치로 로컬 추첨한다.
	if (bYang2ClientAuthorityActive && !bInInstance && Type != EUEGachaType::None)
	{
		FUEFieldGachaResult Result;
		UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
		const UUEProjectAssets* Assets = UUEProjectAssetSettings::GetProjectAssets();
		UUEGachaPool* Pool = nullptr;
		if (Assets)
		{
			for (const TSoftObjectPtr<UUEGachaPool>& Candidate : Assets->GachaPools)
			{
				UUEGachaPool* Loaded = Candidate.LoadSynchronous();
				if (Loaded && Loaded->GetServerType() == Type)
				{
					Pool = Loaded;
					break;
				}
			}
		}

		double TotalWeight = 0.0;
		if (Pool)
		{
			for (const FUEGachaEntry& Entry : Pool->Entries)
			{
				if (Entry.DexNumber > 0 && FMath::IsFinite(Entry.Weight) && Entry.Weight > 0.0f)
				{
					TotalWeight += Entry.Weight;
				}
			}
		}

		if (!GameInstance || !Pool || TotalWeight <= 0.0)
		{
			Result.Message = TEXT("Yang2 로컬 뽑기 데이터가 비어 있습니다");
			QueueYang2GachaResult(Result);
			return true;
		}
		if (!GameInstance->SpendYang2PokemonToken())
		{
			Result.Message = TEXT("포켓몬 토큰이 없습니다");
			QueueYang2GachaResult(Result);
			return true;
		}

		double Roll = FMath::FRandRange(0.0f, static_cast<float>(TotalWeight));
		const FUEGachaEntry* Drawn = nullptr;
		for (const FUEGachaEntry& Entry : Pool->Entries)
		{
			if (Entry.DexNumber <= 0 || !FMath::IsFinite(Entry.Weight) || Entry.Weight <= 0.0f)
			{
				continue;
			}
			Roll -= Entry.Weight;
			if (Roll <= 0.0)
			{
				Drawn = &Entry;
				break;
			}
		}
		if (!Drawn)
		{
			for (int32 Index = Pool->Entries.Num() - 1; Index >= 0; --Index)
			{
				const FUEGachaEntry& Entry = Pool->Entries[Index];
				if (Entry.DexNumber > 0 && FMath::IsFinite(Entry.Weight) && Entry.Weight > 0.0f)
				{
					Drawn = &Entry;
					break;
				}
			}
		}

		if (Drawn)
		{
			Result.bOk = true;
			Result.Dex = Drawn->DexNumber;
			Result.Rarity = Drawn->Rarity;
			Result.bDuplicate = !GameInstance->UnlockYang2Pokemon(Drawn->DexNumber);
			Result.Message = Result.bDuplicate ? TEXT("이미 보유한 포켓몬입니다")
			                                           : TEXT("새 포켓몬을 해금했습니다");
		}
		PokemonTokens = GameInstance->GetYang2PokemonTokens();
		OnTokenBalanceChanged.Broadcast(PokemonTokens);
		RefreshYang2PartyState(true, FString());
		QueueYang2GachaResult(Result);
		return true;
	}

	if (!FieldConnection || !FieldConnection->IsInField() || Type == EUEGachaType::None)
	{
		return false;
	}

	FieldConnection->SendGachaDraw(static_cast<uint8>(Type));
	return true;
}

bool UUEFieldServerBridgeComponent::SendDebugGrantToken()
{
	// YANG2_CLIENT_AUTHORITY_ONLY
	if (bYang2ClientAuthorityActive && !bInInstance)
	{
		if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance()))
		{
			GameInstance->GrantYang2PokemonToken();
			PokemonTokens = GameInstance->GetYang2PokemonTokens();
			OnTokenBalanceChanged.Broadcast(PokemonTokens);
			return true;
		}
	}

	if (!FieldConnection || !FieldConnection->IsInField())
	{
		return false;
	}

	FieldConnection->SendDebugGrantToken();
	return true;
}

void UUEFieldServerBridgeComponent::QueueYang2GachaResult(const FUEFieldGachaResult& Result)
{
	// 요청 함수가 반환된 뒤 기계가 bAwaitingResult를 켜므로 다음 틱에 답한다.
	TWeakObjectPtr<UUEFieldServerBridgeComponent> WeakThis(this);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([WeakThis, Result]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnGachaResult.Broadcast(Result);
			}
		});
	}
}

void UUEFieldServerBridgeComponent::HandleFieldGachaResult(const FHHVFieldGachaResult &Result)
{
	FUEFieldGachaResult Payload;
	Payload.bOk = Result.bOk;
	Payload.Message = Result.Message;
	Payload.Dex = static_cast<int32>(Result.Dex);
	Payload.Rarity = Result.Rarity <= static_cast<uint8>(EUEGachaRarity::SuperRare)
		? static_cast<EUEGachaRarity>(Result.Rarity)
		: EUEGachaRarity::Normal;
	Payload.bDuplicate = Result.bDuplicate;

	OnGachaResult.Broadcast(Payload);
}

void UUEFieldServerBridgeComponent::HandleFieldTokenBalance(uint32 Tokens)
{
	PokemonTokens = static_cast<int32>(FMath::Min<uint32>(Tokens, MAX_int32));
	OnTokenBalanceChanged.Broadcast(PokemonTokens);
}

void UUEFieldServerBridgeComponent::HandleInstanceWeatherState(
	const FHHVInstanceWeatherState &Weather)
{
	// 이전 방에서 늦게 도착한 패킷이 새 방 연출을 덮지 못하게 방 번호도 확인한다.
	if (!bInInstance || CurrentRoomId == 0 || Weather.RoomId != CurrentRoomId)
	{
		return;
	}

	InstanceWeatherState.RoomId = static_cast<int32>(Weather.RoomId);
	InstanceWeatherState.Revision = static_cast<int32>(Weather.Revision);
	InstanceWeatherState.SimulationTimeSeconds = Weather.SimulationTimeSeconds;
	InstanceWeatherState.TemperatureC = Weather.TemperatureC;
	InstanceWeatherState.RelativeHumidityPct = Weather.RelativeHumidityPct;
	InstanceWeatherState.PressureHpa = Weather.PressureHpa;
	InstanceWeatherState.CloudCover = Weather.CloudCover;
	InstanceWeatherState.PrecipitationMmPerHour = Weather.PrecipitationMmPerHour;
	InstanceWeatherState.WindSpeedMps = Weather.WindSpeedMps;
	InstanceWeatherState.WindDirectionDegrees = Weather.WindDirectionDegrees;
	InstanceWeatherState.GroundWetness = Weather.GroundWetness;
	InstanceWeatherState.SnowDepthM = Weather.SnowDepthM;
	InstanceWeatherState.WaterBalanceErrorKgM2 = Weather.WaterBalanceErrorKgM2;
	bHasInstanceWeatherState = true;

	OnInstanceWeatherChanged.Broadcast(InstanceWeatherState);
}

AUEPlayerCharacter *UUEFieldServerBridgeComponent::GetPlayerCharacter() const
{
	return CachedPlayerCharacter.Get();
}

void UUEFieldServerBridgeComponent::ReportFieldPositionNow()
{
	const AUEPlayerCharacter *PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter || !FieldConnection || !FieldConnection->IsInField())
	{
		return;
	}

	// 전송 간격을 건너뛴다. 다음 정기 전송을 기다릴 여유가 없다 — 곧 레벨이
	// 갈리면서 이 연결이 통째로 사라진다.
	auto *Movement = GetPlayerCharacter()->GetCoreMovement();
	FieldConnection->SendMove(Movement->TakeNetworkInputs(), Movement->GetInputEpoch(), Movement->NeedsInputReset());
	TimeSinceLastSend = 0.0f;
}

void UUEFieldServerBridgeComponent::HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation,
                                                                   FVector OldVelocity)
{
	(void)OldLocation;
	(void)OldVelocity;

	const AUEPlayerCharacter *PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter || !PlayerCharacter->IsLocallyControlled())
	{
		return;
	}

	if (!FieldConnection || !FieldConnection->IsInField())
	{
		return;
	}

	TimeSinceLastSend += DeltaSeconds;
	if (TimeSinceLastSend < SendIntervalSeconds)
	{
		return;
	}
	TimeSinceLastSend = 0.0f;

	auto *Movement = GetPlayerCharacter()->GetCoreMovement();
	FieldConnection->SendMove(Movement->TakeNetworkInputs(), Movement->GetInputEpoch(), Movement->NeedsInputReset());
}
