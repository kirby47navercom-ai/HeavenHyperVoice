// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPlayerMovementSyncComponent.h"

#include "../../Character/UEPlayerCharacter.h"
#include "../../Pokemon/Server/UEPokemonServerComponent.h"
#include "../../Pokemon/UEPokemonCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Paths.h"

UUEPlayerMovementSyncComponent::UUEPlayerMovementSyncComponent()
{
	// Only the field-server path needs a tick, to drain the network queue. It is
	// switched on in BeginPlay so the local-validation path costs nothing.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	static ConstructorHelpers::FClassFinder<AUEPokemonCharacter> WildClassFinder(TEXT("/Game/Pokemon/BP_Pokemon"));
	if (WildClassFinder.Succeeded())
	{
		WildPokemonClass = WildClassFinder.Class;
	}
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

	PlayerCharacter->OnCharacterMovementUpdated.AddDynamic(this, &ThisClass::HandleCharacterMovementUpdated);
	SaveLastValidatedServerState(PlayerCharacter->GetActorLocation(), PlayerCharacter->GetVelocity(), PlayerCharacter->GetActorRotation());

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
	// 이 컴포넌트가 BP 에 배치돼 있으면 BP 저장값(None)이 생성자의 FClassFinder 를
	// 덮어써 WildPokemonClass 가 비어버린다. 그러면 야생을 한 마리도 못 그린다.
	// 런타임에 직접 로드해 확실히 채운다.
	if (!WildPokemonClass)
	{
		WildPokemonClass = LoadClass<AUEPokemonCharacter>(
			nullptr, TEXT("/Game/Pokemon/BP_Pokemon.BP_Pokemon_C"));
		UE_LOG(LogTemp, Warning, TEXT("[WILD] WildPokemonClass fallback load: %s"),
			WildPokemonClass ? TEXT("ok") : TEXT("FAILED"));
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
		if (OnFieldSnapshot)
		{
			OnFieldSnapshot(Snapshot);
		}
	};

	FHHVFieldSettings Settings;
	Settings.Host = FieldServerHost;
	Settings.Port = FieldServerPort;
	Settings.DevName = DevCharacterName;
	Settings.DevCharacterId = static_cast<uint64>(DevCharacterId);
	FieldConnection->Start(Settings);

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
	SaveLastValidatedServerState(SpawnPosition, FVector::ZeroVector, SpawnRotation);

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

	// 새로 시야에 들어온 야생 포켓몬을 스폰한다. 다른 플레이어(Species==0)는
	// 아직 다루지 않는다.
	int32 Made = 0, SkippedSpecies = 0, SkippedExisting = 0, SkippedNoClass = 0, FailedSpawn = 0;
	for (const FHHVFieldEntity& Entity : Snapshot.Spawned)
	{
		if (Entity.Species == 0) { ++SkippedSpecies; continue; }
		if (WildActors.Contains(Entity.EntityId)) { ++SkippedExisting; continue; }
		if (!WildPokemonClass) { ++SkippedNoClass; continue; }

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
			++FailedSpawn;
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
		++Made;

		const FVector SpawnLoc = SpawnTransform.GetLocation();
		UE_LOG(LogTemp, Warning,
			TEXT("[WILD] spawn id=%llu unreal=(%.0f, %.0f, %.0f) server=(%.0f, %.0f)"),
			Entity.EntityId, SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z, Entity.X, Entity.Y);

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
			continue;
		}
		// ponytail: 좌표를 즉시 박는다. 20Hz 라 조금 끊겨 보이면 목표를 두고
		//           Tick 에서 보간하면 되지만, 지금은 눈에 띄는지부터 본다.
		// 목표만 넘긴다. 액터의 Tick(UpdateServerDrivenMovement)이 VInterpTo 로
		// 서서히 이동하므로, 20Hz 스냅샷 사이가 부드럽게 채워진다.
		AUEPokemonCharacter* WildActor = Found->Get();
		WildActor->ApplyServerMoveTarget(MakeLocation(Entity.X, Entity.Y), FVector::ZeroVector,
			FRotator(0.0f, Entity.Facing, 0.0f), /*bTeleported=*/false);
	}

	// 시야에서 나갔거나 사라진 야생 포켓몬을 제거한다.
	int32 Removed = 0;
	for (const uint64 EntityId : Snapshot.Despawned)
	{
		TWeakObjectPtr<AUEPokemonCharacter> WildActor;
		if (WildActors.RemoveAndCopyValue(EntityId, WildActor))
		{
			++Removed;
			if (WildActor.IsValid())
			{
				WildActor->Destroy();
			}
		}
	}

	if (Snapshot.Spawned.Num() > 0 || Snapshot.Despawned.Num() > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WILD] in.spawned=%d made=%d skip(sp=%d exist=%d noclass=%d fail=%d) | despawned=%d removed=%d | live=%d class=%s"),
			Snapshot.Spawned.Num(), Made, SkippedSpecies, SkippedExisting, SkippedNoClass, FailedSpawn,
			Snapshot.Despawned.Num(), Removed, WildActors.Num(),
			WildPokemonClass ? TEXT("ok") : TEXT("NULL"));
	}
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

FUEPlayerMovementPacket UUEPlayerMovementSyncComponent::BuildMovementPacket(float DeltaSeconds)
{
	FUEPlayerMovementPacket MovementPacket;

	AUEPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return MovementPacket;
	}

	MovementPacket.Sequence = NextMoveSequence++;
	MovementPacket.DeltaSeconds = DeltaSeconds;
	MovementPacket.MoveInput = PlayerCharacter->GetMovementInput();
	MovementPacket.ClientPosition = PlayerCharacter->GetActorLocation();
	MovementPacket.ClientVelocity = PlayerCharacter->GetVelocity();
	MovementPacket.ControlRotation = PlayerCharacter->GetControlRotation();
	MovementPacket.ActorRotation = PlayerCharacter->GetActorRotation();

	return MovementPacket;
}

void UUEPlayerMovementSyncComponent::RecordMovementPacket(const FUEPlayerMovementPacket& MovementPacket)
{
	FUEPlayerMovementHistoryEntry HistoryEntry;
	HistoryEntry.Packet = MovementPacket;
	HistoryEntry.ReportedPosition = MovementPacket.ClientPosition;
	HistoryEntry.ReportedVelocity = MovementPacket.ClientVelocity;
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
		SaveLastValidatedServerState(MovementPacket.ClientPosition, MovementPacket.ClientVelocity, MovementPacket.ActorRotation);
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
		SaveLastValidatedServerState(OutServerPosition, OutServerVelocity, OutServerRotation);
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

	if (!bTryLoadDefaultServerMap)
	{
		return FString();
	}

	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("ServerMaps"),
		TEXT("PlayerTestLevel.hhvservermap")
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

void UUEPlayerMovementSyncComponent::SaveLastValidatedServerState(const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation)
{
	LastValidatedServerPosition = ServerPosition;
	LastValidatedServerVelocity = ServerVelocity;
	LastValidatedServerRotation = ServerRotation;
	bLastValidatedServerStateValid = true;
}

HHV::Map::Vec3 UUEPlayerMovementSyncComponent::ToServerVec3(const FVector& Vector)
{
	return HHV::Map::Vec3{
		static_cast<float>(Vector.X),
		static_cast<float>(Vector.Y),
		static_cast<float>(Vector.Z)
	};
}

FVector UUEPlayerMovementSyncComponent::ToUnrealVector(const HHV::Map::Vec3& Vector)
{
	return FVector(Vector.X, Vector.Y, Vector.Z);
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

	const FUEPlayerMovementPacket MovementPacket = BuildMovementPacket(DeltaSeconds);
	RecordMovementPacket(MovementPacket);
	SendMovementPacketToServer(MovementPacket);
}
