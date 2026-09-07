#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/Runnable.h"

struct ssl_ctx_st;
struct ssl_st;
struct bio_st;

struct FHHVChatSettings
{
	FString Host;
	int32 Port = 0;
	TArray<uint8> Ticket;
};

/**
 * 발화가 속한 채널. chat.fbs 의 HeavenChat::Channel 과 값이 같아야 한다.
 *
 * System 은 와이어에 없다 — Notice 로 따로 오는 것을 화면에서 한 줄로 섞기
 * 위해 클라이언트가 붙이는 값이다. 시스템 줄은 어느 탭에서나 보인다.
 */
enum class EUEChatChannel : uint8
{
	General = 0,
	Party = 1,

	// 지금 들어와 있는 인스턴스 방 안에서만 오간다. 화면의 "전투" 탭이 이것이다.
	Instance = 2,

	System = 3
};

enum class EHHVChatEvent : uint8
{
	Notice,
	Message,
	PartyState,
	PartyInvited,
	PartyInstanceReady,
	Disconnected
};

/** 플레이어 파티 한 명. 포켓몬 파티와는 다른 것이다. */
struct FHHVPartyMember
{
	uint64 AccountId = 0;
	FString Nickname;
};

struct FHHVChatEventData
{
	EHHVChatEvent Type = EHHVChatEvent::Disconnected;
	FString Nickname;
	FString Text;
	EUEChatChannel Channel = EUEChatChannel::General;

	// 파티 이벤트에서만 쓴다. PartyId 가 0 이면 파티가 없다는 뜻이다.
	uint64 PartyId = 0;
	TArray<FHHVPartyMember> Members;
	uint32 InstanceType = 0;
};

/** ChatServer 한 세션을 소유하는 TLS 클라이언트다. */
class HEAVENHYPERVOICE_API FHHVChatConnection : public FRunnable
{
public:
	FHHVChatConnection() = default;
	virtual ~FHHVChatConnection() override;

	FHHVChatConnection(const FHHVChatConnection&) = delete;
	FHHVChatConnection& operator=(const FHHVChatConnection&) = delete;

	void Start(const FHHVChatSettings& InSettings);
	void Shutdown();

	/** 공백과 서버의 1 KiB 상한을 검사하고 전송 큐에 넣는다. */
	bool SendSay(const FString& Text, FString& OutError);

	/** 파티원에게만 보낸다. 파티가 없으면 서버가 안내를 돌려준다. */
	bool SendSayParty(const FString& Text, FString& OutError);

	/** 같은 인스턴스 방 사람에게만 보낸다. 밖이면 서버가 안내를 돌려준다. */
	bool SendSayInstance(const FString& Text, FString& OutError);

	// --- 파티 ---
	//
	// 파티를 바꾸는 명령은 전부 채팅 연결로 간다. 필드에서든 인스턴스에서든
	// 이 연결은 살아 있고, 쓰기가 한 서버에만 있으면 경쟁 조건도 한 곳에만 생긴다.

	void SendPartyInvite(const FString& TargetNickname);
	void SendPartyAccept(uint64 PartyId);
	void SendPartyDecline(uint64 PartyId);
	void SendPartyLeave();
	void SendPartyKick(uint64 TargetAccountId);

	/** 파티장만 부를 수 있다. 전원에게 입장 신호가 나간다. */
	void SendPartyEnterInstance(uint32 InstanceType);
	static bool ValidateText(const FString& Text, FString& OutError);

	/** 게임 스레드에서 호출한다. */
	void Poll();

	TFunction<void(const FString& Text)> OnNotice;
	TFunction<void(const FString& Nickname, const FString& Text, EUEChatChannel Channel)> OnMessage;
	TFunction<void(const FString& Reason)> OnDisconnected;

	// PartyId 가 0 이면 파티가 없다. 화면을 지우라는 뜻이다.
	TFunction<void(uint64 PartyId, const TArray<FHHVPartyMember>& Members, const FString& Message)> OnPartyState;
	TFunction<void(uint64 PartyId, const FString& FromNickname)> OnPartyInvited;
	TFunction<void(uint32 InstanceType)> OnPartyInstanceReady;

protected:
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

	FHHVChatSettings Settings;
	FRunnableThread* Thread = nullptr;
	FThreadSafeBool bStopRequested = false;
	FThreadSafeBool bConnected = false;
	FThreadSafeBool bDisconnectPushed = false;

	ssl_ctx_st* Ctx = nullptr;
	ssl_st* Ssl = nullptr;
	bio_st* Bio = nullptr;

	TArray<uint8> RecvAccum;
	TArray<uint8> PendingWrite;
	int32 PendingWriteOffset = 0;

	TQueue<TArray<uint8>, EQueueMode::Spsc> Outbound;
	TQueue<FHHVChatEventData, EQueueMode::Spsc> Inbound;
};
