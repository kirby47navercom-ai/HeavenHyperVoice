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

enum class EHHVChatEvent : uint8
{
	Notice,
	Message,
	Disconnected
};

struct FHHVChatEventData
{
	EHHVChatEvent Type = EHHVChatEvent::Disconnected;
	FString Nickname;
	FString Text;
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
	static bool ValidateText(const FString& Text, FString& OutError);

	/** 게임 스레드에서 호출한다. */
	void Poll();

	TFunction<void(const FString& Text)> OnNotice;
	TFunction<void(const FString& Nickname, const FString& Text)> OnMessage;
	TFunction<void(const FString& Reason)> OnDisconnected;

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
