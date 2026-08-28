// Field server transport. TLS over TCP, 4-byte little-endian length prefix,
// FlatBuffers payloads -- the same wire format the login and chat servers use.
//
// Everything OpenSSL touches happens on one worker thread. The game thread only
// pushes bytes into a queue and drains events out of another, so no SSL object
// is ever seen by two threads at once.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/Runnable.h"

struct ssl_ctx_st;
struct ssl_st;
struct bio_st;

/** One entity as the server reported it. Nickname and species arrive on spawn only. */
struct FHHVFieldEntity
{
	uint64 EntityId = 0;
	float X = 0.0f;
	float Y = 0.0f;
	float Facing = 0.0f;
	FString Nickname;
	uint16 PartnerSpecies = 0;

	// 야생 포켓몬의 종족. 0 이면 플레이어 캐릭터다.
	uint16 Species = 0;
};

struct FHHVFieldSnapshot
{
	TArray<FHHVFieldEntity> Spawned;
	TArray<FHHVFieldEntity> Moved;
	TArray<uint64> Despawned;
};

/** 서버가 보내 준 파티 상태. 입장 직후 한 번, SetParty 응답으로 한 번 온다. */
struct FHHVFieldPartyState
{
	bool bOk = false;
	FString Message;

	// 전부 도감번호다. Party 의 순서가 슬롯 번호다.
	TArray<uint16> Party;
	uint16 ActiveDex = 0;

	// 해금한 종족 전부. 파티를 고르는 화면의 후보 목록이다.
	TArray<uint16> Unlocked;
};

enum class EHHVFieldEvent : uint8
{
	EnterAck,
	Correction,
	Snapshot,
	Notice,
	PartyState,
	PartnerChanged,
	Disconnected
};

struct FHHVFieldEventData
{
	EHHVFieldEvent Type = EHHVFieldEvent::Disconnected;
	uint64 EntityId = 0;
	uint32 Sequence = 0;
	float X = 0.0f;
	float Y = 0.0f;
	float Facing = 0.0f;
	FString Text;
	FHHVFieldSnapshot Snapshot;
	FHHVFieldPartyState Party;

	// PartnerChanged 에서만 쓴다. 도감번호이고 0 이면 도로 넣었다는 뜻이다.
	uint16 PartnerDex = 0;
};

struct FHHVFieldSettings
{
	FString Host;
	int32 Port = 0;

	/**
	 * LoginServer 가 audience:"field" 로 서명한 티켓. 비어 있지 않으면 이것만
	 * 보내고 dev_* 는 싣지 않는다.
	 */
	TArray<uint8> Ticket;

	/** Ticket 이 비었을 때만 쓴다. 서버가 --dev-no-auth 로 떠 있어야 통한다. */
	FString DevName;
	uint64 DevCharacterId = 0;
	uint16 DevPartnerSpecies = 0;
};

/**
 * Owns the worker thread for one field session.
 *
 * Start() connects and sends Enter. Poll() must be called on the game thread;
 * it drains decoded frames and fires the callbacks below. Callbacks are never
 * invoked from the worker thread.
 */
class HEAVENHYPERVOICE_API FHHVFieldConnection : public FRunnable
{
public:
	FHHVFieldConnection() = default;
	virtual ~FHHVFieldConnection() override;

	FHHVFieldConnection(const FHHVFieldConnection&) = delete;
	FHHVFieldConnection& operator=(const FHHVFieldConnection&) = delete;

	void Start(const FHHVFieldSettings& InSettings);
	void Shutdown();

	/** True once EnterAck has been drained by Poll(). */
	bool IsInField() const { return bInField; }

	void SendMove(float X, float Y, float Facing, uint32 Sequence);

	/**
	 * 파티 구성과 꺼낼 한 마리를 정한다. 전부 도감번호다.
	 *
	 * 캐릭터 번호를 싣지 않는다 — 서버는 티켓으로 이미 누구인지 안다.
	 * 응답은 OnPartyState 로 온다. 거절이어도 현재 상태가 함께 온다.
	 */
	void SendSetParty(const TArray<uint16>& DexNumbers, uint16 ActiveDex);

	/** Game thread. Drains the inbound queue and fires the callbacks. */
	void Poll();

	TFunction<void(uint64 EntityId, float X, float Y, float Facing)> OnEnterAck;
	TFunction<void(uint32 Sequence, float X, float Y, float Facing)> OnCorrection;
	TFunction<void(const FHHVFieldSnapshot& Snapshot)> OnSnapshot;
	TFunction<void(const FString& Text)> OnNotice;
	TFunction<void(const FHHVFieldPartyState& State)> OnPartyState;
	TFunction<void(uint64 EntityId, uint16 PartnerDex)> OnPartnerChanged;
	TFunction<void(const FString& Reason)> OnDisconnected;

protected:
	// FRunnable
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	bool ConnectAndHandshake(FString& OutError);
	void CloseTls();
	bool FlushOutbound();
	bool ReadInbound(FString& OutError);
	void ParseAccumulated();
	void DispatchFrame(const uint8* Data, int32 Size);
	void PushDisconnect(const FString& Reason);

	FHHVFieldSettings Settings;

	FRunnableThread* Thread = nullptr;
	FThreadSafeBool bStopRequested = false;
	FThreadSafeBool bDisconnectPushed = false;
	bool bInField = false;

	ssl_ctx_st* Ctx = nullptr;
	ssl_st* Ssl = nullptr;
	bio_st* Bio = nullptr;

	// Worker thread only.
	TArray<uint8> RecvAccum;
	TArray<uint8> PendingWrite;
	int32 PendingWriteOffset = 0;

	TQueue<TArray<uint8>, EQueueMode::Spsc> Outbound;
	TQueue<FHHVFieldEventData, EQueueMode::Spsc> Inbound;
};
