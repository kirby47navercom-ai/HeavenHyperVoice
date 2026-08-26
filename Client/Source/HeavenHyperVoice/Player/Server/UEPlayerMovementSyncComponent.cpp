// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPlayerMovementSyncComponent.h"

#include "../../Character/UEPlayerCharacter.h"
#include "../../Pokemon/Server/UEPokemonServerComponent.h"
#include "../../Pokemon/UEPokemonCharacter.h"
#include "../../System/UEGameInstance.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

UUEPlayerMovementSyncComponent::UUEPlayerMovementSyncComponent()
{
	// Only the field-server path needs a tick, to drain the network queue. It is
	// switched on in BeginPlay so the local-validation path costs nothing.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

}

void UUEPlayerMovementSyncComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedPlayerCharacter = Cast<AUEPlayerCharacter>(GetOwner());
	AUEPlayerCharacter* PlayerCharacter = CachedPlayerCharacter.Get();
	if (!PlayerCharacter)
	{
		return;
	}

	// 로비 카드의 UViewport 같은 미리보기 세계에서는 네트워크 시뮬레이션을 실행하지 않는다.
	const UWorld* OwnerWorld = GetWorld();
	if (!OwnerWorld || (OwnerWorld->WorldType != EWorldType::Game && OwnerWorld->WorldType != EWorldType::PIE))
	{
		SetComponentTickEnabled(false);
		return;
	}

	// 다른 플레이어를 그리는 복제본에도 이 컴포넌트가 딸려 온다. 여기서
	// 물러나지 않으면 시야에 들어온 사람 수만큼 필드 서버에 TLS 연결이 열린다.
	if (PlayerCharacter->IsRemoteProxy())
	{
		SetComponentTickEnabled(false);
		return;
	}

	PlayerCharacter->OnCharacterMovementUpdated.AddDynamic(this, &ThisClass::HandleCharacterMovementUpdated);
	SaveLastValidatedServerState(PlayerCharacter->GetActorLocation(), PlayerCharacter->GetActorRotation());

	// 프런트엔드의 로컬 로그인으로 입장한 동안에는 필드 서버 연결을 시작하지 않는다.
	const UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetWorld()->GetGameInstance());
	if (GameInstance && GameInstance->HasLocalSession())
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (bEnableLocalServerValidation)
	{
		TryLoadServerMap();
		return;
	}

	StartFieldConnection();
}

void UUEPlayerMovementSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter())
	{
		PlayerCharacter->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleCharacterMovementUpdated);
	}

	// Joins the worker thread. Do it before the callbacks below can dangle.
	FieldConnection.reset();
	DestroyWildActors();
	DestroyRemotePlayers();

	Super::EndPlay(EndPlayReason);
}

void UUEPlayerMovementSyncComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (FieldConnection)
	{
		FieldConnection->Poll();
	}
}

void UUEPlayerMovementSyncComponent::StartFieldConnection()
{
	if (FieldServerHost.IsEmpty() || FieldServerPort <= 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("PlayerMovementSync: FieldServerHost 또는 FieldServerPort가 DefaultGame.ini에 설정되지 않았습니다."));
		return;
	}

	if (!WildPokemonClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[WILD] WildPokemonClass is not assigned in the MovementSync component Blueprint defaults."));
	}

	FieldConnection = std::make_unique<FHHVFieldConnection>();

	// The connection lives inside this component and is destroyed in EndPlay
	// before anything else tears down, so capturing `this` is safe. Poll() only
	// runs from TickComponent, so these all land on the game thread.
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

	// 여러 대에서 같은 필드에 붙는 테스트를 하려면 클라마다 번호가 달라야 한다
	// (DefaultDevCharacterId 주석 참고). 명령줄로 정하거나, 안 주면 머신마다
	// 다른 값을 만들어 쓴다.
	//
	//   HeavenHyperVoice.exe -DevCharId=9002 -DevName=노트북
	uint64 ResolvedCharacterId = static_cast<uint64>(DevCharacterId);
	int32 CharIdOverride = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("DevCharId="), CharIdOverride) && CharIdOverride > 0)
	{
		ResolvedCharacterId = static_cast<uint64>(CharIdOverride);
	}
	else if (DevCharacterId == DefaultDevCharacterId)
	{
		// 기본값 그대로다. 머신/사용자마다 고유한 문자열에서 뽑아 두 대가 저절로
		// 갈리게 한다. 같은 머신에서는 실행할 때마다 같은 번호가 나온다.
		// 야생 엔티티 번호(kWildIdBase = 1<<52)와 겹치지 않게 작게 유지한다.
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
		// 로그와 화면에서 누가 누군지 구분되게 번호를 붙인다.
		ResolvedName = FString::Printf(TEXT("UEClient-%llu"), ResolvedCharacterId);
	}

	FHHVFieldSettings Settings;
	Settings.Host = FieldServerHost;
	Settings.Port = FieldServerPort;
	Settings.DevName = ResolvedName;
	Settings.DevCharacterId = ResolvedCharacterId;

	// 로그인 서버를 거쳐 왔으면 티켓과 접속 주소가 GameInstance 에 들어 있다.
	// 그때는 위의 dev 값과 에디터에 박아둔 호스트를 쓰지 않는다 — 어디로 붙을지는
	// 서버가 정한다.
	if (const UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetWorld()->GetGameInstance()))
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

	FieldConnection->Start(Settings);

	UE_LOG(LogTemp, Display, TEXT("PlayerMovementSync: connecting to %s:%d as %s (id %llu)"),
		*Settings.Host, Settings.Port, *Settings.DevName, Settings.DevCharacterId);

	SetComponentTickEnabled(true);
}

void UUEPlayerMovementSyncComponent::HandleFieldEnterAck(uint64 EntityId, float ServerX, float ServerY, float Facing)
{
	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return;
	}

	// The server decides where entering puts you -- it restores the last saved
	// position. Height is ours; the server has no Z yet.
	FVector SpawnPosition = PlayerCharacter->GetActorLocation();
	SpawnPosition.X = ToUnrealAxis(ServerX);
	SpawnPosition.Y = ToUnrealAxis(ServerY);

	FRotator SpawnRotation = PlayerCharacter->GetActorRotation();
	SpawnRotation.Yaw = Facing;

	PlayerCharacter->ApplyServerMovementCorrection(SpawnPosition, FVector::ZeroVector, SpawnRotation, true);
	SaveLastValidatedServerState(SpawnPosition, SpawnRotation);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("PlayerMovementSync: entered field as entity %llu at (%.0f, %.0f)"),
		EntityId,
		SpawnPosition.X,
		SpawnPosition.Y
	);
}

void UUEPlayerMovementSyncComponent::HandleFieldCorrection(uint32 Sequence, float ServerX, float ServerY, float Facing)
{
	// Corrections only arrive when the server changed the coordinates, so there
	// is no ack for an ordinary move. History is trimmed by MaxMoveHistoryEntries
	// instead, and a correction older than that window is simply gone.
	const int32 HistoryIndex = FindMoveHistoryIndex(Sequence);
	if (HistoryIndex == INDEX_NONE)
	{
		return;
	}

	const FUEPlayerMovementHistoryEntry& HistoryEntry = MoveHistory[HistoryIndex];

	FVector ServerPosition = HistoryEntry.ReportedPosition;
	ServerPosition.X = ToUnrealAxis(ServerX);
	ServerPosition.Y = ToUnrealAxis(ServerY);

	FRotator ServerRotation = HistoryEntry.ReportedRotation;
	ServerRotation.Yaw = Facing;

	// A correction means the move was refused, so whatever velocity carried us
	// there is wrong too. Zero it and let local input build it back up.
	HandleServerMovementResult(Sequence, ServerPosition, FVector::ZeroVector, ServerRotation);
}

void UUEPlayerMovementSyncComponent::HandleFieldSnapshot(const FHHVFieldSnapshot& Snapshot)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 서버는 2D 라 높이가 없다. 플레이어와 같은 바닥에 세운다.
	const AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	const double GroundZ = PlayerCharacter ? PlayerCharacter->GetActorLocation().Z : 0.0;

	const auto MakeLocation = [&](float ServerX, float ServerY)
	{
		return FVector(ToUnrealAxis(ServerX), ToUnrealAxis(ServerY), GroundZ);
	};

	// 새로 시야에 들어온 야생 포켓몬을 스폰한다.
	for (const FHHVFieldEntity& Entity : Snapshot.Spawned)
	{
		if (Entity.Species == 0)
		{
			// 다른 플레이어다. 서버는 자기 자신을 빼고 보내므로 여기 내가 섞일
			// 일은 없다 (World::updateVisibility 가 self 를 건너뛴다).
			SpawnRemotePlayer(Entity, MakeLocation(Entity.X, Entity.Y));
			continue;
		}
		if (WildActors.Contains(Entity.EntityId) || !WildPokemonClass)
		{
			continue;
		}

		// 지연 스폰으로 만든다. AutoPossessAI 는 스폰 직후 프레임에 컨트롤러를
		// 빙의시키는데, 그 로컬 AI(FollowOwner 계열)가 야생을 플레이어 쪽으로
		// 끌고 가 버린다. BeginPlay 전에 AI 를 꺼야 빙의 자체가 일어나지 않는다.
		const FTransform SpawnTransform(FRotator(0.0f, Entity.Facing, 0.0f),
			MakeLocation(Entity.X, Entity.Y));
		AUEPokemonCharacter* WildActor = World->SpawnActorDeferred<AUEPokemonCharacter>(
			WildPokemonClass, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!WildActor)
		{
			continue;
		}
		WildActor->AutoPossessAI = EAutoPossessAI::Disabled;
		WildActor->AIControllerClass = nullptr;

		// 액터에 붙은 로컬 서버 시뮬레이션도 꺼야 한다. 기본값이
		// bEnableServer=true / FollowOwner 라, 이걸 두면 AI 컨트롤러와는 **별개
		// 경로**로 야생이 매 틱 플레이어 쪽으로 걸어온다. 시야에 들어오는 족족
		// 플레이어에게 몰려와 겹쳐 쌓이는 원인이었다.
		// 야생 좌표는 필드 서버가 전부 지시한다 (아래 ApplyServerMoveTarget).
		if (UUEPokemonServerComponent* ServerComponent = WildActor->GetServerComponent())
		{
			ServerComponent->SetServerSimulationEnabled(false);
		}

		WildActor->FinishSpawning(SpawnTransform);

		// 서버가 위치를 완전히 지시하므로 로컬 충돌도 끈다. 겹쳐 스폰돼도 서로
		// 밀치지 않는다.
		WildActor->SetActorEnableCollision(false);

		WildActor->SetWildSpecies(static_cast<int32>(Entity.Species));
		WildActors.Add(Entity.EntityId, WildActor);
	}

	// 움직인 야생 포켓몬의 위치를 갱신한다. moved 에는 종족이 실리지 않으므로
	// 이미 야생으로 스폰해 둔 것만 본다.
	for (const FHHVFieldEntity& Entity : Snapshot.Moved)
	{
		const TWeakObjectPtr<AUEPokemonCharacter>* Found = WildActors.Find(Entity.EntityId);
		if (!Found || !Found->IsValid())
		{
			// 야생이 아니면 다른 플레이어다. moved 에는 종족이 실리지 않으므로
			// 어느 쪽 표에 있는지로만 구분한다.
			if (const TWeakObjectPtr<AUEPlayerCharacter>* Remote = RemotePlayers.Find(Entity.EntityId))
			{
				if (Remote->IsValid())
				{
					Remote->Get()->ApplyRemoteMoveTarget(MakeLocation(Entity.X, Entity.Y),
						FRotator(0.0f, Entity.Facing, 0.0f), /*bTeleported=*/false);
				}
			}
			continue;
		}
		// 목표만 넘긴다. 액터의 Tick(UpdateServerDrivenMovement)이 VInterpTo 로
		// 서서히 이동하므로, 20Hz 스냅샷 사이가 부드럽게 채워진다.
		AUEPokemonCharacter* WildActor = Found->Get();
		WildActor->ApplyServerMoveTarget(MakeLocation(Entity.X, Entity.Y), FVector::ZeroVector,
			FRotator(0.0f, Entity.Facing, 0.0f), /*bTeleported=*/false);
	}

	// 시야에서 나갔거나 사라진 엔티티를 제거한다.
	for (const uint64 EntityId : Snapshot.Despawned)
	{
		TWeakObjectPtr<AUEPokemonCharacter> WildActor;
		if (WildActors.RemoveAndCopyValue(EntityId, WildActor))
		{
			if (WildActor.IsValid())
			{
				WildActor->Destroy();
			}
			continue;
		}

		TWeakObjectPtr<AUEPlayerCharacter> RemotePlayer;
		if (RemotePlayers.RemoveAndCopyValue(EntityId, RemotePlayer) && RemotePlayer.IsValid())
		{
			RemotePlayer->Destroy();
		}
	}
}

void UUEPlayerMovementSyncComponent::SpawnRemotePlayer(const FHHVFieldEntity& Entity,
	const FVector& SpawnLocation)
{
	if (RemotePlayers.Contains(Entity.EntityId))
	{
		return;
	}

	UWorld* World = GetWorld();
	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!World || !PlayerCharacter)
	{
		return;
	}

	// 로컬 플레이어와 **같은 클래스**를 그대로 쓴다. 메시도 커마도 이미 맞춰져
	// 있으니 원격 전용 BP 를 따로 만들고 동기화할 이유가 없다.
	const FTransform SpawnTransform(FRotator(0.0f, Entity.Facing, 0.0f), SpawnLocation);
	AUEPlayerCharacter* Proxy = World->SpawnActorDeferred<AUEPlayerCharacter>(
		PlayerCharacter->GetClass(), SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Proxy)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FIELD] remote player %llu spawn failed"), Entity.EntityId);
		return;
	}

	// FinishSpawning 전에 표시해야 한다. BeginPlay 가 지나면 이 복제본이
	// 자기 필드 연결을 열고 동행 포켓몬까지 부른 뒤다.
	Proxy->MakeRemoteProxy();
	Proxy->FinishSpawning(SpawnTransform);

	RemotePlayers.Add(Entity.EntityId, Proxy);
}

void UUEPlayerMovementSyncComponent::DestroyRemotePlayers()
{
	for (TPair<uint64, TWeakObjectPtr<AUEPlayerCharacter>>& Pair : RemotePlayers)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}
	RemotePlayers.Empty();
}

void UUEPlayerMovementSyncComponent::DestroyWildActors()
{
	for (TPair<uint64, TWeakObjectPtr<AUEPokemonCharacter>>& Pair : WildActors)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}
	WildActors.Empty();
}

FUEPlayerMovementPacket UUEPlayerMovementSyncComponent::BuildMovementPacket()
{
	FUEPlayerMovementPacket MovementPacket;

	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return MovementPacket;
	}

	MovementPacket.Sequence = NextMoveSequence++;
	MovementPacket.ClientPosition = PlayerCharacter->GetActorLocation();
	MovementPacket.ClientVelocity = PlayerCharacter->GetVelocity();
	MovementPacket.ActorRotation = PlayerCharacter->GetActorRotation();

	return MovementPacket;
}

void UUEPlayerMovementSyncComponent::RecordMovementPacket(const FUEPlayerMovementPacket& MovementPacket)
{
	FUEPlayerMovementHistoryEntry HistoryEntry;
	HistoryEntry.Packet = MovementPacket;
	HistoryEntry.ReportedPosition = MovementPacket.ClientPosition;
	HistoryEntry.ReportedRotation = MovementPacket.ActorRotation;
	MoveHistory.Add(MoveTemp(HistoryEntry));

	// Keep only recent reported states so latency spikes cannot grow memory forever.
	while (MoveHistory.Num() > MaxMoveHistoryEntries)
	{
		MoveHistory.RemoveAt(0, 1, EAllowShrinking::No);
	}
}

void UUEPlayerMovementSyncComponent::SendMovementPacketToServer(const FUEPlayerMovementPacket& MovementPacket)
{
	if (bEnableLocalServerValidation)
	{
		ValidateMovementPacketOnLocalServer(MovementPacket);
		return;
	}

	if (!FieldConnection || !FieldConnection->IsInField())
	{
		return;
	}

	FieldConnection->SendMove(
		ToServerAxis(MovementPacket.ClientPosition.X),
		ToServerAxis(MovementPacket.ClientPosition.Y),
		static_cast<float>(MovementPacket.ActorRotation.Yaw),
		MovementPacket.Sequence
	);
}

void UUEPlayerMovementSyncComponent::TryLoadServerMap()
{
	const FString MapFilePath = ResolveServerMapFilePath();
	if (MapFilePath.IsEmpty() || !FPaths::FileExists(MapFilePath))
	{
		bServerMapLoaded = false;
		UE_LOG(LogTemp, Warning, TEXT("PlayerMovementSync: server map not found: %s"), *MapFilePath);
		return;
	}

	bServerMapLoaded = ServerMapRuntime.LoadFromFile(TCHAR_TO_UTF8(*MapFilePath));
	UE_LOG(
		LogTemp,
		Display,
		TEXT("PlayerMovementSync: server map %s: %s"),
		bServerMapLoaded ? TEXT("loaded") : TEXT("failed"),
		*MapFilePath
	);
}

void UUEPlayerMovementSyncComponent::ValidateMovementPacketOnLocalServer(const FUEPlayerMovementPacket& MovementPacket)
{
	FVector ServerPosition = MovementPacket.ClientPosition;
	FVector ServerVelocity = MovementPacket.ClientVelocity;
	FRotator ServerRotation = MovementPacket.ActorRotation;

	BuildLocalServerMovementResult(MovementPacket, ServerPosition, ServerVelocity, ServerRotation);
	HandleServerMovementResult(MovementPacket.Sequence, ServerPosition, ServerVelocity, ServerRotation);
}

bool UUEPlayerMovementSyncComponent::BuildLocalServerMovementResult(const FUEPlayerMovementPacket& MovementPacket, FVector& OutServerPosition, FVector& OutServerVelocity, FRotator& OutServerRotation)
{
	if (!bServerMapLoaded)
	{
		SaveLastValidatedServerState(MovementPacket.ClientPosition, MovementPacket.ActorRotation);
		OutServerPosition = MovementPacket.ClientPosition;
		OutServerVelocity = MovementPacket.ClientVelocity;
		OutServerRotation = MovementPacket.ActorRotation;
		return true;
	}

	const HHV::Map::AgentSettings Agent = MakeAgentSettings();
	const HHV::Map::FloorSample Floor = ServerMapRuntime.SampleFloor(
		static_cast<float>(MovementPacket.ClientPosition.X),
		static_cast<float>(MovementPacket.ClientPosition.Y),
		Agent.WalkableFloorAngleDegrees
	);
	const HHV::Map::Vec3 ServerCapsuleLocation{
		static_cast<float>(MovementPacket.ClientPosition.X),
		static_cast<float>(MovementPacket.ClientPosition.Y),
		Floor.FloorZ + Agent.CapsuleHalfHeight
	};
	const bool bHasGroundAtXY = Floor.bHit;
	const bool bBlockedByWall = bHasGroundAtXY && ServerMapRuntime.IsBlockedByWall(ServerCapsuleLocation, Agent);
	if (bHasGroundAtXY && !bBlockedByWall)
	{
		// Valid movement is approved exactly as the client reported it.
		OutServerPosition = MovementPacket.ClientPosition;
		OutServerVelocity = MovementPacket.ClientVelocity;
		OutServerRotation = MovementPacket.ActorRotation;
		SaveLastValidatedServerState(OutServerPosition, OutServerRotation);
		return true;
	}

	// If the reported state is outside the server map, keep the last server-approved state.
	OutServerPosition = bLastValidatedServerStateValid ? LastValidatedServerPosition : MovementPacket.ClientPosition;
	OutServerVelocity = FVector::ZeroVector;
	OutServerRotation = bLastValidatedServerStateValid ? LastValidatedServerRotation : MovementPacket.ActorRotation;
	return false;
}

FString UUEPlayerMovementSyncComponent::ResolveServerMapFilePath() const
{
	if (!ServerMapFilePath.IsEmpty())
	{
		return ServerMapFilePath;
	}

	if (!bTryLoadDefaultServerMap || DefaultServerMapFileName.IsEmpty())
	{
		return FString();
	}

	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("ServerMaps"),
		DefaultServerMapFileName
	);
}

HHV::Map::AgentSettings UUEPlayerMovementSyncComponent::MakeAgentSettings() const
{
	HHV::Map::AgentSettings Agent;
	const AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return Agent;
	}

	if (const UCapsuleComponent* CapsuleComponent = PlayerCharacter->GetCapsuleComponent())
	{
		Agent.CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
		Agent.CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	}

	if (const UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
	{
		Agent.MaxStepHeight = MovementComponent->MaxStepHeight;
		Agent.WalkableFloorAngleDegrees = MovementComponent->GetWalkableFloorAngle();
	}

	return Agent;
}

void UUEPlayerMovementSyncComponent::HandleServerMovementResult(uint32 AckSequence, const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation)
{
	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return;
	}

	const int32 HistoryIndex = FindMoveHistoryIndex(AckSequence);
	if (HistoryIndex == INDEX_NONE)
	{
		return;
	}

	const FUEPlayerMovementHistoryEntry& HistoryEntry = MoveHistory[HistoryIndex];
	const float CorrectionDistance = FVector::Dist(HistoryEntry.ReportedPosition, ServerPosition);
	if (CorrectionDistance <= ServerCorrectionTolerance)
	{
		PruneMoveHistory(HistoryIndex);
		return;
	}

	// Server validation wins when the reported state is outside the allowed error range.
	const bool bUseHardCorrection = CorrectionDistance >= HardCorrectionDistance;
	PlayerCharacter->ApplyServerMovementCorrection(ServerPosition, ServerVelocity, ServerRotation, bUseHardCorrection);
	PruneMoveHistory(HistoryIndex);
}

void UUEPlayerMovementSyncComponent::PruneMoveHistory(int32 LastConfirmedIndex)
{
	if (LastConfirmedIndex >= 0)
	{
		MoveHistory.RemoveAt(0, LastConfirmedIndex + 1, EAllowShrinking::No);
	}
}

int32 UUEPlayerMovementSyncComponent::FindMoveHistoryIndex(uint32 Sequence) const
{
	for (int32 Index = 0; Index < MoveHistory.Num(); ++Index)
	{
		if (MoveHistory[Index].Packet.Sequence == Sequence)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

AUEPlayerCharacter* UUEPlayerMovementSyncComponent::GetPlayerCharacter() const
{
	if (CachedPlayerCharacter.IsValid())
	{
		return CachedPlayerCharacter.Get();
	}

	return Cast<AUEPlayerCharacter>(GetOwner());
}

void UUEPlayerMovementSyncComponent::SaveLastValidatedServerState(const FVector& ServerPosition, const FRotator& ServerRotation)
{
	LastValidatedServerPosition = ServerPosition;
	LastValidatedServerRotation = ServerRotation;
	bLastValidatedServerStateValid = true;
}

void UUEPlayerMovementSyncComponent::HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	(void)OldLocation;
	(void)OldVelocity;

	const AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter || !PlayerCharacter->IsLocallyControlled())
	{
		return;
	}

	// Movement updates fire every frame. The server drops anything closer than
	// 10ms apart and only broadcasts at 20Hz, so sending per frame just burns
	// bandwidth and leaves history entries no correction will ever reference.
	if (!bEnableLocalServerValidation)
	{
		TimeSinceLastSend += DeltaSeconds;
		if (TimeSinceLastSend < SendIntervalSeconds)
		{
			return;
		}
		TimeSinceLastSend = 0.0f;
	}

	const FUEPlayerMovementPacket MovementPacket = BuildMovementPacket();
	RecordMovementPacket(MovementPacket);
	SendMovementPacketToServer(MovementPacket);
}
