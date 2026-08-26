#include "HHVLoginConnection.h"

#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"

// OpenSSL declares `typedef struct ui_st UI;` at global scope and Unreal already
// has a `UI` namespace, so including the two together is a redefinition error.
// Renaming OpenSSL's type out of the way is the usual fix; nothing here touches
// its UI API.
#define UI OPENSSL_UI

THIRD_PARTY_INCLUDES_START
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
THIRD_PARTY_INCLUDES_END

#undef UI

THIRD_PARTY_INCLUDES_START
#include "flatbuffers/flatbuffers.h"

#include "Generated/login_generated.h"
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogHHVLogin, Log, All);

namespace
{
	// 이름에 Login 을 붙이는 이유는 언리얼 유니티 빌드 때문이다. 여러 .cpp 가 한
	// 번역 단위로 합쳐지면 익명 네임스페이스라도 같은 이름끼리 충돌한다
	// (HHVFieldConnection.cpp 에 같은 역할의 헬퍼가 있다).

	/** Frames are tiny. Anything larger means the length prefix is garbage. */
	constexpr int32 LoginMaxFrameBytes = 64 * 1024;

	/** Nothing to read most iterations, so do not spin the core. */
	constexpr float LoginIdleSleepSeconds = 0.005f;

	FString LastLoginSslError()
	{
		const unsigned long Code = ERR_get_error();
		if (Code == 0)
		{
			return TEXT("no OpenSSL error queued");
		}

		char Buffer[256] = {};
		ERR_error_string_n(Code, Buffer, sizeof(Buffer));
		return FString(UTF8_TO_TCHAR(Buffer));
	}

	/** Prefixes a finished FlatBuffer with its little-endian length. */
	TArray<uint8> LoginFrameOf(const flatbuffers::FlatBufferBuilder& Builder)
	{
		const uint32 Size = Builder.GetSize();

		TArray<uint8> Frame;
		Frame.SetNumUninitialized(4 + static_cast<int32>(Size));
		Frame[0] = static_cast<uint8>(Size & 0xFF);
		Frame[1] = static_cast<uint8>((Size >> 8) & 0xFF);
		Frame[2] = static_cast<uint8>((Size >> 16) & 0xFF);
		Frame[3] = static_cast<uint8>((Size >> 24) & 0xFF);
		FMemory::Memcpy(Frame.GetData() + 4, Builder.GetBufferPointer(), Size);
		return Frame;
	}

	FString ReadString(const flatbuffers::String* Value)
	{
		return Value ? FString(UTF8_TO_TCHAR(Value->c_str())) : FString();
	}

	/** 서버 Appearance -> 커마 구조체. 없으면 기본값 그대로 둔다. */
	FUEHHVAppearance ReadAppearance(const HeavenLogin::Appearance* Wire)
	{
		FUEHHVAppearance Out;
		if (Wire == nullptr)
		{
			return Out;
		}

		Out.Gender = Wire->gender() != 0 ? EUEHHVGender::TypeB : EUEHHVGender::TypeA;
		Out.BodyIndex = Wire->body();
		Out.HeadIndex = Wire->head();
		Out.HairIndex = Wire->hair();
		Out.EyeIndex = Wire->eye();
		Out.BodyEquipmentIndex = Wire->equipment();
		Out.SkinColor = FLinearColor(Wire->skin_r(), Wire->skin_g(), Wire->skin_b(), 1.0f);
		Out.HairColor = FLinearColor(Wire->hair_r(), Wire->hair_g(), Wire->hair_b(), 1.0f);
		Out.EyeColor = FLinearColor(Wire->eye_r(), Wire->eye_g(), Wire->eye_b(), 1.0f);
		Out.ArmVolume = Wire->arm_volume();
		Out.TorsoVolume = Wire->torso_volume();
		Out.LegVolume = Wire->leg_volume();
		return Out;
	}

	/** 커마 구조체 -> 서버 Appearance. 상위 테이블을 시작하기 전에 불러야 한다. */
	flatbuffers::Offset<HeavenLogin::Appearance> BuildAppearance(
		flatbuffers::FlatBufferBuilder& Builder, const FUEHHVAppearance& Appearance)
	{
		HeavenLogin::AppearanceBuilder Out(Builder);
		Out.add_gender(Appearance.Gender == EUEHHVGender::TypeB ? 1 : 0);
		Out.add_body(Appearance.BodyIndex);
		Out.add_head(Appearance.HeadIndex);
		Out.add_hair(Appearance.HairIndex);
		Out.add_eye(Appearance.EyeIndex);
		Out.add_equipment(Appearance.BodyEquipmentIndex);
		Out.add_skin_r(Appearance.SkinColor.R);
		Out.add_skin_g(Appearance.SkinColor.G);
		Out.add_skin_b(Appearance.SkinColor.B);
		Out.add_hair_r(Appearance.HairColor.R);
		Out.add_hair_g(Appearance.HairColor.G);
		Out.add_hair_b(Appearance.HairColor.B);
		Out.add_eye_r(Appearance.EyeColor.R);
		Out.add_eye_g(Appearance.EyeColor.G);
		Out.add_eye_b(Appearance.EyeColor.B);
		Out.add_arm_volume(Appearance.ArmVolume);
		Out.add_torso_volume(Appearance.TorsoVolume);
		Out.add_leg_volume(Appearance.LegVolume);
		return Out.Finish();
	}

	void ReadCharacters(
		const flatbuffers::Vector<flatbuffers::Offset<HeavenLogin::CharacterSummary>>* Source,
		TArray<FHHVCharacterSummary>& Out)
	{
		if (Source == nullptr)
		{
			return;
		}

		Out.Reserve(static_cast<int32>(Source->size()));
		for (const HeavenLogin::CharacterSummary* Entry : *Source)
		{
			if (Entry == nullptr)
			{
				continue;
			}

			FHHVCharacterSummary& Character = Out.AddDefaulted_GetRef();
			Character.Id = Entry->id();
			Character.Nickname = ReadString(Entry->nickname());
			Character.Appearance = ReadAppearance(Entry->appearance());

			if (const HeavenLogin::PokemonSummary* Partner = Entry->partner())
			{
				Character.bHasPartner = true;
				Character.Partner.SpeciesId = Partner->species_id();
				Character.Partner.DexNumber = Partner->dex_number();
				Character.Partner.Nickname = ReadString(Partner->nickname());
				Character.Partner.Level = Partner->level();
				Character.Partner.MaxHP = Partner->max_hp();
				Character.Partner.Atk = Partner->atk();
				Character.Partner.Def = Partner->def();
				Character.Partner.SpAtk = Partner->sp_atk();
				Character.Partner.SpDef = Partner->sp_def();
				Character.Partner.Speed = Partner->speed();
				Character.Partner.IvHP = Partner->iv_hp();
				Character.Partner.IvAtk = Partner->iv_atk();
				Character.Partner.IvDef = Partner->iv_def();
				Character.Partner.IvSpAtk = Partner->iv_sp_atk();
				Character.Partner.IvSpDef = Partner->iv_sp_def();
				Character.Partner.IvSpeed = Partner->iv_speed();
			}
		}
	}
}

FHHVLoginConnection::~FHHVLoginConnection()
{
	Shutdown();
}

void FHHVLoginConnection::Start(const FHHVLoginSettings& InSettings)
{
	if (Thread != nullptr)
	{
		return;
	}

	Settings = InSettings;
	bStopRequested = false;
	bDisconnectPushed = false;
	bRequestInFlight = false;
	bAuthenticated = false;
	Thread = FRunnableThread::Create(this, TEXT("HHVLoginConnection"), 0, TPri_BelowNormal);
}

void FHHVLoginConnection::Shutdown()
{
	bStopRequested = true;
	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
	bAuthenticated = false;
	bRequestInFlight = false;
}

void FHHVLoginConnection::Stop()
{
	bStopRequested = true;
}

void FHHVLoginConnection::Enqueue(TArray<uint8>&& Frame)
{
	if (bStopRequested)
	{
		return;
	}

	// 서버는 응답 전에 다음 프레임이 오면 연결을 끊는다 (LoginHandler 의 Busy 단계).
	// 여기서 막지 않으면 버튼 연타 한 번에 세션이 날아간다.
	if (bRequestInFlight)
	{
		UE_LOG(LogHHVLogin, Warning, TEXT("request already in flight, dropping this one"));
		return;
	}

	bRequestInFlight = true;
	Outbound.Enqueue(MoveTemp(Frame));
}

void FHHVLoginConnection::SendLogin(const FString& UserId, const FString& Password)
{
	flatbuffers::FlatBufferBuilder Builder(256);
	const auto User = Builder.CreateString(TCHAR_TO_UTF8(*UserId));
	const auto Secret = Builder.CreateString(TCHAR_TO_UTF8(*Password));
	const auto Request = HeavenLogin::CreateLoginRequest(Builder, User, Secret);
	Builder.Finish(HeavenLogin::CreateEnvelope(
		Builder, HeavenLogin::Payload::LoginRequest, Request.Union()));

	// 비밀번호는 로그에 남기지 않는다. 아이디만 남겨도 추적에 충분하다.
	UE_LOG(LogHHVLogin, Display, TEXT("login as %s"), *UserId);
	Enqueue(LoginFrameOf(Builder));
}

void FHHVLoginConnection::SendRegister(const FString& UserId, const FString& Password)
{
	flatbuffers::FlatBufferBuilder Builder(256);
	const auto User = Builder.CreateString(TCHAR_TO_UTF8(*UserId));
	const auto Secret = Builder.CreateString(TCHAR_TO_UTF8(*Password));
	const auto Request = HeavenLogin::CreateRegisterRequest(Builder, User, Secret);
	Builder.Finish(HeavenLogin::CreateEnvelope(
		Builder, HeavenLogin::Payload::RegisterRequest, Request.Union()));

	UE_LOG(LogHHVLogin, Display, TEXT("register as %s"), *UserId);
	Enqueue(LoginFrameOf(Builder));
}

void FHHVLoginConnection::SendCreateCharacter(const FString& Nickname, uint16 DexNumber,
	const FUEHHVAppearance& Appearance)
{
	flatbuffers::FlatBufferBuilder Builder(512);
	const auto Nick = Builder.CreateString(TCHAR_TO_UTF8(*Nickname));
	const auto Look = BuildAppearance(Builder, Appearance);

	HeavenLogin::CreateCharacterRequestBuilder Request(Builder);
	Request.add_nickname(Nick);
	// 서버는 도감번호를 받아 내부 번호로 바꾼다. species_id 는 채우지 않는다.
	Request.add_dex_number(DexNumber);
	Request.add_appearance(Look);
	Builder.Finish(HeavenLogin::CreateEnvelope(
		Builder, HeavenLogin::Payload::CreateCharacterRequest, Request.Finish().Union()));

	Enqueue(LoginFrameOf(Builder));
}

void FHHVLoginConnection::SendDeleteCharacter(uint64 CharacterId, const FString& ConfirmNickname)
{
	flatbuffers::FlatBufferBuilder Builder(256);
	const auto Nick = Builder.CreateString(TCHAR_TO_UTF8(*ConfirmNickname));
	const auto Request = HeavenLogin::CreateDeleteCharacterRequest(Builder, CharacterId, Nick);
	Builder.Finish(HeavenLogin::CreateEnvelope(
		Builder, HeavenLogin::Payload::DeleteCharacterRequest, Request.Union()));

	Enqueue(LoginFrameOf(Builder));
}

void FHHVLoginConnection::SendReleasePartner(uint64 CharacterId)
{
	flatbuffers::FlatBufferBuilder Builder(128);
	const auto Request = HeavenLogin::CreateReleasePartnerRequest(Builder, CharacterId);
	Builder.Finish(HeavenLogin::CreateEnvelope(
		Builder, HeavenLogin::Payload::ReleasePartnerRequest, Request.Union()));

	Enqueue(LoginFrameOf(Builder));
}

void FHHVLoginConnection::SendSelectCharacter(uint64 CharacterId)
{
	flatbuffers::FlatBufferBuilder Builder(128);
	const auto Request = HeavenLogin::CreateSelectCharacterRequest(Builder, CharacterId);
	Builder.Finish(HeavenLogin::CreateEnvelope(
		Builder, HeavenLogin::Payload::SelectCharacterRequest, Request.Union()));

	Enqueue(LoginFrameOf(Builder));
}

uint32 FHHVLoginConnection::Run()
{
	FString Error;
	if (!ConnectAndHandshake(Error))
	{
		PushDisconnect(Error);
		CloseTls();
		return 0;
	}

	while (!bStopRequested)
	{
		if (!FlushOutbound())
		{
			PushDisconnect(TEXT("send failed: ") + LastLoginSslError());
			break;
		}

		if (!ReadInbound(Error))
		{
			PushDisconnect(Error);
			break;
		}

		FPlatformProcess::Sleep(LoginIdleSleepSeconds);
	}

	CloseTls();
	PushDisconnect(TEXT("closed"));
	return 0;
}

bool FHHVLoginConnection::ConnectAndHandshake(FString& OutError)
{
	Ctx = SSL_CTX_new(TLS_client_method());
	if (Ctx == nullptr)
	{
		OutError = TEXT("SSL_CTX_new failed: ") + LastLoginSslError();
		return false;
	}

	SSL_CTX_set_min_proto_version(Ctx, TLS1_2_VERSION);

	// 필드 연결과 같은 자체 서명 개발 인증서라 검증할 체인이 없다.
	// 배포 전에 SSL_VERIFY_PEER + CA 고정으로 바꿔야 한다. 로그인은 비밀번호가
	// 흐르는 경로라 필드보다 더 급하다.
	SSL_CTX_set_verify(Ctx, SSL_VERIFY_NONE, nullptr);

	Bio = BIO_new_ssl_connect(Ctx);
	if (Bio == nullptr)
	{
		OutError = TEXT("BIO_new_ssl_connect failed: ") + LastLoginSslError();
		return false;
	}

	BIO_get_ssl(Bio, &Ssl);
	if (Ssl == nullptr)
	{
		OutError = TEXT("BIO_get_ssl failed: ") + LastLoginSslError();
		return false;
	}

	// Retries reuse PendingWrite, which can be reallocated between attempts.
	SSL_set_mode(Ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	const FString Address = FString::Printf(TEXT("%s:%d"), *Settings.Host, Settings.Port);
	BIO_set_conn_hostname(Bio, TCHAR_TO_UTF8(*Address));

	if (BIO_do_connect(Bio) <= 0)
	{
		OutError = FString::Printf(TEXT("cannot reach login server at %s: %s"),
			*Address, *LastLoginSslError());
		return false;
	}

	BIO_socket_nbio(BIO_get_fd(Bio, nullptr), 1);

	UE_LOG(LogHHVLogin, Display, TEXT("connected to %s (%s, %s)"), *Address,
		UTF8_TO_TCHAR(SSL_get_version(Ssl)), UTF8_TO_TCHAR(SSL_get_cipher(Ssl)));
	return true;
}

void FHHVLoginConnection::CloseTls()
{
	if (Bio != nullptr)
	{
		BIO_free_all(Bio);
		Bio = nullptr;
		Ssl = nullptr;  // owned by the BIO chain
	}
	if (Ctx != nullptr)
	{
		SSL_CTX_free(Ctx);
		Ctx = nullptr;
	}
}

bool FHHVLoginConnection::FlushOutbound()
{
	for (;;)
	{
		if (PendingWrite.Num() == 0)
		{
			TArray<uint8> Next;
			if (!Outbound.Dequeue(Next))
			{
				return true;
			}
			PendingWrite = MoveTemp(Next);
			PendingWriteOffset = 0;
		}

		const int32 Remaining = PendingWrite.Num() - PendingWriteOffset;
		const int Written = SSL_write(Ssl, PendingWrite.GetData() + PendingWriteOffset, Remaining);
		if (Written > 0)
		{
			PendingWriteOffset += Written;
			if (PendingWriteOffset >= PendingWrite.Num())
			{
				PendingWrite.Reset();
				PendingWriteOffset = 0;
			}
			continue;
		}

		const int Reason = SSL_get_error(Ssl, Written);
		if (Reason == SSL_ERROR_WANT_WRITE || Reason == SSL_ERROR_WANT_READ)
		{
			// Socket buffer is full. The same bytes go out next iteration.
			return true;
		}
		return false;
	}
}

bool FHHVLoginConnection::ReadInbound(FString& OutError)
{
	uint8 Buffer[8192];
	for (;;)
	{
		const int Read = SSL_read(Ssl, Buffer, sizeof(Buffer));
		if (Read > 0)
		{
			RecvAccum.Append(Buffer, Read);
			ParseAccumulated();
			continue;
		}

		const int Reason = SSL_get_error(Ssl, Read);
		if (Reason == SSL_ERROR_WANT_READ || Reason == SSL_ERROR_WANT_WRITE)
		{
			return true;
		}
		if (Reason == SSL_ERROR_ZERO_RETURN)
		{
			// 캐릭터를 고른 뒤에는 서버가 정상적으로 끊는다. 오류가 아니다.
			OutError = TEXT("server closed the connection");
			return false;
		}

		OutError = TEXT("recv failed: ") + LastLoginSslError();
		return false;
	}
}

void FHHVLoginConnection::ParseAccumulated()
{
	int32 Offset = 0;
	while (RecvAccum.Num() - Offset >= 4)
	{
		const uint8* Header = RecvAccum.GetData() + Offset;
		const uint32 Size = static_cast<uint32>(Header[0]) | (static_cast<uint32>(Header[1]) << 8) |
			(static_cast<uint32>(Header[2]) << 16) | (static_cast<uint32>(Header[3]) << 24);

		if (Size == 0 || Size > LoginMaxFrameBytes)
		{
			UE_LOG(LogHHVLogin, Warning, TEXT("frame length %u out of range, dropping stream"), Size);
			RecvAccum.Reset();
			bStopRequested = true;
			return;
		}

		if (static_cast<uint32>(RecvAccum.Num() - Offset - 4) < Size)
		{
			break;
		}

		DispatchFrame(RecvAccum.GetData() + Offset + 4, static_cast<int32>(Size));
		Offset += 4 + static_cast<int32>(Size);
	}

	if (Offset > 0)
	{
		RecvAccum.RemoveAt(0, Offset, EAllowShrinking::No);
	}
}

void FHHVLoginConnection::DispatchFrame(const uint8* Data, int32 Size)
{
	// 신뢰할 수 없는 입력으로 다루는 것이 맞다. 잘린 프레임도 GetRoot 에는
	// 그냥 오프셋으로 들어간다.
	flatbuffers::Verifier Verifier(Data, static_cast<size_t>(Size));
	if (!HeavenLogin::VerifyEnvelopeBuffer(Verifier))
	{
		UE_LOG(LogHHVLogin, Warning, TEXT("malformed login frame (%d bytes)"), Size);
		return;
	}

	const HeavenLogin::Envelope* Envelope = HeavenLogin::GetEnvelope(Data);

	// Verifier 는 payload_type 만 있고 payload 오프셋이 없는 프레임을 통과시킨다
	// (VerifyTable(nullptr) 이 true 다). 그대로 두면 아래 payload_as_* 가 nullptr 을
	// 돌려주고 바로 역참조해 죽는다.
	if (Envelope == nullptr || Envelope->payload() == nullptr)
	{
		UE_LOG(LogHHVLogin, Warning, TEXT("login frame carried no payload (%d bytes)"), Size);
		return;
	}

	FHHVLoginEventData Event;
	switch (Envelope->payload_type())
	{
	case HeavenLogin::Payload::LoginResponse:
	{
		const HeavenLogin::LoginResponse* Response = Envelope->payload_as_LoginResponse();
		Event.Type = EHHVLoginEvent::LoginResponse;
		Event.bOk = Response->ok();
		Event.Message = ReadString(Response->message());
		Event.MaxSlots = Response->max_slots();
		ReadCharacters(Response->characters(), Event.Characters);
		break;
	}

	case HeavenLogin::Payload::RegisterResponse:
	{
		const HeavenLogin::RegisterResponse* Response = Envelope->payload_as_RegisterResponse();
		Event.Type = EHHVLoginEvent::RegisterResponse;
		Event.bOk = Response->ok();
		Event.Message = ReadString(Response->message());
		break;
	}

	case HeavenLogin::Payload::CharacterListResponse:
	{
		const HeavenLogin::CharacterListResponse* Response =
			Envelope->payload_as_CharacterListResponse();
		Event.Type = EHHVLoginEvent::CharacterList;
		Event.bOk = Response->ok();
		Event.Message = ReadString(Response->message());
		ReadCharacters(Response->characters(), Event.Characters);
		break;
	}

	case HeavenLogin::Payload::SelectCharacterResponse:
	{
		const HeavenLogin::SelectCharacterResponse* Response =
			Envelope->payload_as_SelectCharacterResponse();
		Event.Type = EHHVLoginEvent::SelectResponse;
		Event.bOk = Response->ok();
		Event.Message = ReadString(Response->message());
		Event.Nickname = ReadString(Response->nickname());

		if (const auto* Endpoints = Response->endpoints())
		{
			Event.Endpoints.Reserve(static_cast<int32>(Endpoints->size()));
			for (const HeavenLogin::ServiceEndpoint* Entry : *Endpoints)
			{
				if (Entry == nullptr)
				{
					continue;
				}
				FHHVServiceEndpoint& Endpoint = Event.Endpoints.AddDefaulted_GetRef();
				Endpoint.Service = ReadString(Entry->service());
				Endpoint.Host = ReadString(Entry->host());
				Endpoint.Port = Entry->port();
				if (const auto* Ticket = Entry->ticket())
				{
					Endpoint.Ticket.Append(Ticket->data(), static_cast<int32>(Ticket->size()));
				}
			}
		}
		break;
	}

	default:
		// 나머지는 전부 클라 -> 서버 방향이다. 서버가 보낼 일이 없다.
		UE_LOG(LogHHVLogin, Warning, TEXT("unexpected payload %u from login server"),
			static_cast<uint32>(Envelope->payload_type()));
		return;
	}

	Inbound.Enqueue(MoveTemp(Event));
}

void FHHVLoginConnection::PushDisconnect(const FString& Reason)
{
	// Run() has several exits and the tail runs after every one of them. Only
	// the first reason is worth reporting -- the rest are consequences.
	if (bDisconnectPushed.AtomicSet(true))
	{
		return;
	}

	FHHVLoginEventData Event;
	Event.Type = EHHVLoginEvent::Disconnected;
	Event.Message = Reason;
	Inbound.Enqueue(MoveTemp(Event));
}

void FHHVLoginConnection::Poll()
{
	FHHVLoginEventData Event;
	while (Inbound.Dequeue(Event))
	{
		// 응답이 왔으니 다음 요청을 받을 수 있다. 콜백 안에서 바로 다음 요청을
		// 보내는 흐름이 있으므로 콜백보다 먼저 푼다.
		if (Event.Type != EHHVLoginEvent::Disconnected)
		{
			bRequestInFlight = false;
		}

		switch (Event.Type)
		{
		case EHHVLoginEvent::LoginResponse:
			bAuthenticated = Event.bOk;
			if (OnLoginResponse)
			{
				OnLoginResponse(Event.bOk, Event.Message, Event.Characters, Event.MaxSlots);
			}
			break;

		case EHHVLoginEvent::RegisterResponse:
			if (OnRegisterResponse)
			{
				OnRegisterResponse(Event.bOk, Event.Message);
			}
			break;

		case EHHVLoginEvent::CharacterList:
			if (OnCharacterList)
			{
				OnCharacterList(Event.bOk, Event.Message, Event.Characters);
			}
			break;

		case EHHVLoginEvent::SelectResponse:
			if (OnSelectResponse)
			{
				OnSelectResponse(Event.bOk, Event.Message, Event.Endpoints, Event.Nickname);
			}
			break;

		case EHHVLoginEvent::Disconnected:
			bAuthenticated = false;
			bRequestInFlight = false;
			UE_LOG(LogHHVLogin, Warning, TEXT("disconnected: %s"), *Event.Message);
			if (OnDisconnected)
			{
				OnDisconnected(Event.Message);
			}
			break;
		}
	}
}
