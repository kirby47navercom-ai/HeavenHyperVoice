// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "../../Map/HHVServerMapRuntime.h"
#include "../../Net/HHVFieldConnection.h"

#include <memory>

#include "UEPlayerMovementSyncComponent.generated.h"

class AUEPlayerCharacter;
class AUEPokemonCharacter;

// 서버로 나가는 것은 위치와 각도, 그리고 보정을 되짚을 순번뿐이다.
// 입력이나 카메라 각도는 서버가 보지 않는다.
struct FUEPlayerMovementPacket
{
	uint32 Sequence = 0;
	FVector ClientPosition = FVector::ZeroVector;
	FVector ClientVelocity = FVector::ZeroVector;
	FRotator ActorRotation = FRotator::ZeroRotator;
};

struct FUEPlayerMovementHistoryEntry
{
	FUEPlayerMovementPacket Packet;
	FVector ReportedPosition = FVector::ZeroVector;
	FRotator ReportedRotation = FRotator::ZeroRotator;
};

UCLASS(ClassGroup = (Custom), Config = Game, DefaultConfig, meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEPlayerMovementSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEPlayerMovementSyncComponent();

	void HandleServerMovementResult(uint32 AckSequence, const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	FUEPlayerMovementPacket BuildMovementPacket();
	void RecordMovementPacket(const FUEPlayerMovementPacket& MovementPacket);
	void SendMovementPacketToServer(const FUEPlayerMovementPacket& MovementPacket);
	void TryLoadServerMap();
	void ValidateMovementPacketOnLocalServer(const FUEPlayerMovementPacket& MovementPacket);
	bool BuildLocalServerMovementResult(const FUEPlayerMovementPacket& MovementPacket, FVector& OutServerPosition, FVector& OutServerVelocity, FRotator& OutServerRotation);
	FString ResolveServerMapFilePath() const;
	void StartFieldConnection();
	void HandleFieldEnterAck(uint64 EntityId, float ServerX, float ServerY, float Facing);
	void HandleFieldCorrection(uint32 Sequence, float ServerX, float ServerY, float Facing);

	// 서버 Snapshot 의 원격 엔티티를 액터로 그린다. 지금은 야생 포켓몬(species!=0)
	// 만 스폰하고, 다른 플레이어(species==0)는 아직 다루지 않는다.
	void HandleFieldSnapshot(const FHHVFieldSnapshot& Snapshot);
	void DestroyWildActors();
	void SpawnRemotePlayer(const struct FHHVFieldEntity& Entity, const FVector& SpawnLocation);
	void DestroyRemotePlayers();

	/**
	 * The server world is 51200uu with its origin at a corner, and it clamps
	 * anything outside that box. Unreal levels are built around the origin, so
	 * negative coordinates would be clamped to 0 and corrected forever. Placing
	 * the Unreal origin at the centre of the server world gives +-25600uu of
	 * room in every direction.
	 */
	float ToServerAxis(double UnrealAxis) const { return static_cast<float>(UnrealAxis) + WorldOriginOffset; }
	double ToUnrealAxis(float ServerAxis) const { return static_cast<double>(ServerAxis - WorldOriginOffset); }
	HHV::Map::AgentSettings MakeAgentSettings() const;
	void PruneMoveHistory(int32 LastConfirmedIndex);
	int32 FindMoveHistoryIndex(uint32 Sequence) const;
	AUEPlayerCharacter* GetPlayerCharacter() const;
	void SaveLastValidatedServerState(const FVector& ServerPosition, const FRotator& ServerRotation);

	UFUNCTION()
	void HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync", meta = (ClampMin = "0.0"))
	float ServerCorrectionTolerance = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync", meta = (ClampMin = "0.0"))
	float HardCorrectionDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync", meta = (ClampMin = "1"))
	int32 MaxMoveHistoryEntries = 180;

	/**
	 * Validate against the in-process map runtime instead of the field server.
	 *
	 * This runs inside the client, so it proves the rules but defends nothing --
	 * anyone editing the client skips it. Off by default now that the field
	 * server does the same checks over the wire.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync|Local Server")
	bool bEnableLocalServerValidation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync|Local Server")
	bool bTryLoadDefaultServerMap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync|Local Server")
	FString ServerMapFilePath;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement Sync|Local Server")
	FString DefaultServerMapFileName;

	/** Ignored while bEnableLocalServerValidation is on. */
	// ini(DefaultGame.ini)에서 온다. 로그인 서버를 거쳐 오면 티켓과 함께 받은
	// 주소가 이 값을 덮으므로, 실제로 쓰이는 것은 dev 경로(--dev-no-auth)로
	// 직접 붙을 때뿐이다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Movement Sync|Field Server")
	FString FieldServerHost;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Movement Sync|Field Server", meta = (ClampMin = "1", ClampMax = "65535"))
	int32 FieldServerPort = 0;

	/** Requires the server to run with --dev-no-auth. Replaced by a login ticket later. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Movement Sync|Field Server")
	FString DevCharacterName;

	// 이 값을 그대로 두면 StartFieldConnection 이 머신마다 다른 번호를 만들어 쓴다.
	// --dev-no-auth 서버는 account_id 를 dev_character_id 와 같은 값으로 두고,
	// World::enter 가 계정 단위로 기존 접속을 끊는다. 두 대가 같은 번호로 붙으면
	// 나중에 붙은 쪽이 먼저 붙은 쪽을 튕겨내 서로 보이는 것을 확인할 수 없다.
	static constexpr int64 DefaultDevCharacterId = 9001;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Movement Sync|Field Server", meta = (ClampMin = "1"))
	int64 DevCharacterId = DefaultDevCharacterId;

	/** The server drops moves closer together than 10ms and ticks at 20Hz. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Movement Sync|Field Server", meta = (ClampMin = "0.01"))
	float SendIntervalSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Movement Sync|Field Server")
	float WorldOriginOffset = 0.0f;

	/** 서버 야생 포켓몬을 그릴 액터 클래스. 소유 캐릭터 Blueprint에서 지정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Sync|Field Server")
	TSubclassOf<AUEPokemonCharacter> WildPokemonClass;

private:
	TWeakObjectPtr<AUEPlayerCharacter> CachedPlayerCharacter;
	uint32 NextMoveSequence = 1;
	TArray<FUEPlayerMovementHistoryEntry> MoveHistory;

	std::unique_ptr<FHHVFieldConnection> FieldConnection;

	// 입장 직후 스냅샷 몇 개만 로그로 남기기 위한 카운터. 진단용.
	int32 SnapshotsLogged = 0;
	float TimeSinceLastSend = 0.0f;

	// 서버 entity_id -> 그 야생 포켓몬 액터.
	TMap<uint64, TWeakObjectPtr<AUEPokemonCharacter>> WildActors;

	// 서버 entity_id -> 그 플레이어의 복제본. 클래스는 로컬 플레이어와 같은 것을
	// 그대로 쓰므로 따로 지정할 값이 없다.
	TMap<uint64, TWeakObjectPtr<AUEPlayerCharacter>> RemotePlayers;

	HHV::Map::ServerMapRuntime ServerMapRuntime;
	bool bServerMapLoaded = false;
	bool bLastValidatedServerStateValid = false;
	FVector LastValidatedServerPosition = FVector::ZeroVector;
	FRotator LastValidatedServerRotation = FRotator::ZeroRotator;
};
