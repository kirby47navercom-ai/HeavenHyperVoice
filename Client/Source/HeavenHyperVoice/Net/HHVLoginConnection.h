// Login server transport. TLS over TCP, 4-byte little-endian length prefix,
// FlatBuffers payloads -- the same wire format the field and chat servers use.
//
// Threading matches FHHVFieldConnection: everything OpenSSL touches happens on
// one worker thread, the game thread only pushes bytes into a queue and drains
// events out of another, so no SSL object is ever seen by two threads at once.
//
// Unlike the field connection this one is a conversation, not a stream:
//
//   LoginRequest           -> LoginResponse{characters[]}       (연결 유지)
//   CreateCharacterRequest -> CharacterListResponse             (연결 유지)
//   DeleteCharacterRequest -> CharacterListResponse             (연결 유지)
//   SelectCharacterRequest -> SelectCharacterResponse{tickets}  (서버가 끊는다)
//
// 서버는 한 번에 요청 하나만 받는다. 응답을 기다리지 않고 다음 프레임을 보내면
// 연결이 끊긴다 (LoginHandler 의 Busy 단계). IsBusy() 로 막는다.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/Runnable.h"

#include "../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"

struct ssl_ctx_st;
struct ssl_st;
struct bio_st;

/** 캐릭터에 딸린 포켓몬. 실 수치는 서버가 계산해서 보낸다. */
struct FHHVPokemonSummary
{
	// 서버 내부 번호. 에셋을 찾는 데 쓰지 않는다 — DexNumber 를 쓴다.
	uint16 SpeciesId = 0;

	// 도감번호. 종족 에셋을 찾는 열쇠다.
	uint16 DexNumber = 0;

	// 개체별 닉네임/레벨/개체값은 없어졌다. 저장하는 것은 종족뿐이고, 레벨은
	// 캐릭터가 갖는다 (FHHVCharacterSummary::Level). 능력치는 서버가 캐릭터
	// 레벨과 종족값으로 계산해서 보낸 결과다.
	uint16 MaxHP = 0;
	uint16 Atk = 0;
	uint16 Def = 0;
	uint16 SpAtk = 0;
	uint16 SpDef = 0;
	uint16 Speed = 0;
};

/** 로비에 한 줄로 그려지는 캐릭터. 로컬 슬롯을 대체한다. */
struct FHHVCharacterSummary
{
	uint64 Id = 0;
	FString Nickname;

	// 캐릭터 레벨. 데리고 다니는 포켓몬의 능력치가 이 값에서 나온다.
	uint32 Level = 1;

	bool bHasPartner = false;
	FHHVPokemonSummary Partner;

	// 데리고 다니기로 고른 최대 세 마리. 도감번호이고 순서가 슬롯 번호다.
	// Partner 는 이 중 지금 꺼내 놓은 한 마리다.
	TArray<uint16> Party;

	// 해금한 종족 전부. 도감번호. 파티를 고르는 화면의 후보 목록이다.
	TArray<uint16> Unlocked;

	// 서버가 보관하는 커마 값. 로컬 SaveGame 이 아니라 이쪽이 권위 있는 값이다.
	FUEHHVAppearance Appearance;
};

/** 캐릭터를 고르면 서비스마다 하나씩 온다. 티켓은 그 서비스 전용으로 서명돼 있다. */
struct FHHVServiceEndpoint
{
	FString Service;           // "field" / "chat"
	FString Host;
	uint16 Port = 0;
	TArray<uint8> Ticket;      // 불투명한 바이트열. 클라는 열어보지 않는다.
};

enum class EHHVLoginEvent : uint8
{
	LoginResponse,
	RegisterResponse,
	CharacterList,
	SelectResponse,
	Disconnected
};

struct FHHVLoginEventData
{
	EHHVLoginEvent Type = EHHVLoginEvent::Disconnected;
	bool bOk = false;
	FString Message;
	FString Nickname;
	int32 MaxSlots = 0;
	TArray<FHHVCharacterSummary> Characters;
	TArray<FHHVServiceEndpoint> Endpoints;
};

struct FHHVLoginSettings
{
	FString Host = TEXT("127.0.0.1");
	int32 Port = 9100;
};

/**
 * Owns the worker thread for one login session.
 *
 * Start() connects. The Send* calls queue one request each; Poll() must be
 * called on the game thread and fires the callbacks below. Callbacks are never
 * invoked from the worker thread.
 */
class HEAVENHYPERVOICE_API FHHVLoginConnection : public FRunnable
{
public:
	FHHVLoginConnection() = default;
	virtual ~FHHVLoginConnection() override;

	FHHVLoginConnection(const FHHVLoginConnection&) = delete;
	FHHVLoginConnection& operator=(const FHHVLoginConnection&) = delete;

	void Start(const FHHVLoginSettings& InSettings);
	void Shutdown();

	/** 요청을 보내고 아직 응답을 못 받았다. 이때 또 보내면 서버가 연결을 끊는다. */
	bool IsBusy() const { return bRequestInFlight; }

	/**
	 * 워커가 끝났다. 소켓이 닫혔거나 접속에 실패했다는 뜻이다.
	 *
	 * 객체가 살아 있다고 연결이 살아 있는 것이 아니다. 이걸 안 보고 재사용하면
	 * 요청이 아무도 읽지 않는 큐에 쌓이고 응답이 영영 오지 않는다.
	 */
	bool IsClosed() const { return bDisconnectPushed; }

	/** True once a LoginResponse with ok=true has been drained by Poll(). */
	bool IsAuthenticated() const { return bAuthenticated; }

	void SendLogin(const FString& UserId, const FString& Password);
	void SendRegister(const FString& UserId, const FString& Password);
	void SendCreateCharacter(const FString& Nickname, uint16 DexNumber,
		const FUEHHVAppearance& Appearance);
	void SendDeleteCharacter(uint64 CharacterId, const FString& ConfirmNickname);
	void SendReleasePartner(uint64 CharacterId);

	/**
	 * 파티 구성과 꺼낼 한 마리를 한 번에 보낸다.
	 *
	 * 둘을 나누지 않는 이유는, 파티에서 빼는 순간 꺼내 놓은 것이 파티 밖이 될 수
	 * 있기 때문이다. DexNumbers 는 세 개 이하, 중복 없이, 전부 해금한 것이어야
	 * 하고 ActiveDex 는 그 안에 있어야 한다 (0 이면 아무도 안 꺼낸다).
	 * 서버가 다시 검사하므로 여기서 통과해도 거절될 수 있다.
	 */
	void SendSetParty(uint64 CharacterId, const TArray<uint16>& DexNumbers, uint16 ActiveDex);
	void SendSelectCharacter(uint64 CharacterId);

	/** Game thread. Drains the inbound queue and fires the callbacks. */
	void Poll();

	TFunction<void(bool bOk, const FString& Message, const TArray<FHHVCharacterSummary>& Characters, int32 MaxSlots)> OnLoginResponse;
	TFunction<void(bool bOk, const FString& Message)> OnRegisterResponse;
	TFunction<void(bool bOk, const FString& Message, const TArray<FHHVCharacterSummary>& Characters)> OnCharacterList;
	TFunction<void(bool bOk, const FString& Message, const TArray<FHHVServiceEndpoint>& Endpoints, const FString& Nickname)> OnSelectResponse;
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
	void Enqueue(TArray<uint8>&& Frame);

	FHHVLoginSettings Settings;

	FRunnableThread* Thread = nullptr;
	FThreadSafeBool bStopRequested = false;
	FThreadSafeBool bDisconnectPushed = false;
	FThreadSafeBool bRequestInFlight = false;
	bool bAuthenticated = false;

	ssl_ctx_st* Ctx = nullptr;
	ssl_st* Ssl = nullptr;
	bio_st* Bio = nullptr;

	// Worker thread only.
	TArray<uint8> RecvAccum;
	TArray<uint8> PendingWrite;
	int32 PendingWriteOffset = 0;

	TQueue<TArray<uint8>, EQueueMode::Spsc> Outbound;
	TQueue<FHHVLoginEventData, EQueueMode::Spsc> Inbound;
};
