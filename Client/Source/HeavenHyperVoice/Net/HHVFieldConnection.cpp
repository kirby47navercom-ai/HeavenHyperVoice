#include "HHVFieldConnection.h"

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

#include "Generated/field_generated.h"
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogHHVField, Log, All);

namespace
{
	/** Frames are tiny. Anything larger means the length prefix is garbage. */
	constexpr int32 MaxFrameBytes = 64 * 1024;

	/** Nothing to read most iterations, so do not spin the core. */
	constexpr float IdleSleepSeconds = 0.005f;

	FString LastOpenSslError()
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

	void ReadEntities(const flatbuffers::Vector<flatbuffers::Offset<HeavenField::EntityState>>* Source,
		TArray<FHHVFieldEntity>& Out)
	{
		if (Source == nullptr)
		{
			return;
		}

		Out.Reserve(static_cast<int32>(Source->size()));
		for (const HeavenField::EntityState* State : *Source)
		{
			if (State == nullptr)
			{
				continue;
			}

			FHHVFieldEntity& Entity = Out.AddDefaulted_GetRef();
			Entity.EntityId = State->entity_id();
			Entity.X = State->x();
			Entity.Y = State->y();
			Entity.Facing = State->facing();
			Entity.PartnerSpecies = State->partner_species();
			Entity.Species = State->species();
			if (const flatbuffers::String* Nickname = State->nickname())
			{
				Entity.Nickname = FString(UTF8_TO_TCHAR(Nickname->c_str()));
			}
		}
	}

	/** Prefixes a finished FlatBuffer with its little-endian length. */
	TArray<uint8> FrameOf(const flatbuffers::FlatBufferBuilder& Builder)
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
}

FHHVFieldConnection::~FHHVFieldConnection()
{
	Shutdown();
}

void FHHVFieldConnection::Start(const FHHVFieldSettings& InSettings)
{
	if (Thread != nullptr)
	{
		return;
	}

	Settings = InSettings;
	bStopRequested = false;
	bDisconnectPushed = false;
	Thread = FRunnableThread::Create(this, TEXT("HHVFieldConnection"), 0, TPri_BelowNormal);
}

void FHHVFieldConnection::Shutdown()
{
	bStopRequested = true;
	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
	bInField = false;
}

void FHHVFieldConnection::Stop()
{
	bStopRequested = true;
}

void FHHVFieldConnection::SendMove(float X, float Y, float Facing, uint32 Sequence)
{
	if (bStopRequested)
	{
		return;
	}

	flatbuffers::FlatBufferBuilder Builder(128);
	const auto Move = HeavenField::CreateMove(Builder, X, Y, Facing, Sequence);
	Builder.Finish(HeavenField::CreateEnvelope(Builder, HeavenField::Payload::Move, Move.Union()));
	Outbound.Enqueue(FrameOf(Builder));
}

void FHHVFieldConnection::SendSetParty(const TArray<uint16>& DexNumbers, uint16 ActiveDex)
{
	if (bStopRequested)
	{
		return;
	}

	flatbuffers::FlatBufferBuilder Builder(128);
	// 벡터는 상위 테이블을 시작하기 전에 만들어야 한다.
	const auto Members =
		Builder.CreateVector(DexNumbers.GetData(), static_cast<size_t>(DexNumbers.Num()));
	const auto Request = HeavenField::CreateSetParty(Builder, Members, ActiveDex);
	Builder.Finish(
		HeavenField::CreateEnvelope(Builder, HeavenField::Payload::SetParty, Request.Union()));
	Outbound.Enqueue(FrameOf(Builder));
}

uint32 FHHVFieldConnection::Run()
{
	FString Error;
	if (!ConnectAndHandshake(Error))
	{
		PushDisconnect(Error);
		CloseTls();
		return 0;
	}

	{
		flatbuffers::FlatBufferBuilder Builder(512);

		// 티켓이 있으면 그것만 보낸다. dev_* 를 같이 실으면 서버가 --dev-no-auth
		// 로 떠 있을 때 티켓을 무시하고 dev 경로를 타므로, 어느 쪽으로 들어갔는지
		// 헷갈린다.
		if (Settings.Ticket.Num() > 0)
		{
			const auto Blob = Builder.CreateVector(Settings.Ticket.GetData(), Settings.Ticket.Num());
			const auto Enter = HeavenField::CreateEnter(Builder, Blob);
			Builder.Finish(HeavenField::CreateEnvelope(Builder, HeavenField::Payload::Enter, Enter.Union()));
			UE_LOG(LogHHVField, Display, TEXT("entering with a %d byte ticket"),
				Settings.Ticket.Num());
		}
		else
		{
			const auto Name = Builder.CreateString(TCHAR_TO_UTF8(*Settings.DevName));
			const auto Enter = HeavenField::CreateEnter(Builder, /*ticket=*/0, Name,
				Settings.DevCharacterId, Settings.DevPartnerSpecies);
			Builder.Finish(HeavenField::CreateEnvelope(Builder, HeavenField::Payload::Enter, Enter.Union()));
			UE_LOG(LogHHVField, Warning,
				TEXT("no ticket; entering as dev '%s' (server needs --dev-no-auth)"),
				*Settings.DevName);
		}

		Outbound.Enqueue(FrameOf(Builder));
	}

	while (!bStopRequested)
	{
		if (!FlushOutbound())
		{
			PushDisconnect(TEXT("send failed: ") + LastOpenSslError());
			break;
		}

		if (!ReadInbound(Error))
		{
			PushDisconnect(Error);
			break;
		}

		FPlatformProcess::Sleep(IdleSleepSeconds);
	}

	CloseTls();
	PushDisconnect(TEXT("closed"));
	return 0;
}

bool FHHVFieldConnection::ConnectAndHandshake(FString& OutError)
{
	Ctx = SSL_CTX_new(TLS_client_method());
	if (Ctx == nullptr)
	{
		OutError = TEXT("SSL_CTX_new failed: ") + LastOpenSslError();
		return false;
	}

	SSL_CTX_set_min_proto_version(Ctx, TLS1_2_VERSION);

	// The field server uses the same self-signed development certificate as the
	// login server, so there is no chain to validate against yet. This has to
	// become SSL_VERIFY_PEER with a pinned CA before anything ships.
	SSL_CTX_set_verify(Ctx, SSL_VERIFY_NONE, nullptr);

	Bio = BIO_new_ssl_connect(Ctx);
	if (Bio == nullptr)
	{
		OutError = TEXT("BIO_new_ssl_connect failed: ") + LastOpenSslError();
		return false;
	}

	BIO_get_ssl(Bio, &Ssl);
	if (Ssl == nullptr)
	{
		OutError = TEXT("BIO_get_ssl failed: ") + LastOpenSslError();
		return false;
	}

	// Retries reuse PendingWrite, which can be reallocated between attempts.
	SSL_set_mode(Ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	const FString Address = FString::Printf(TEXT("%s:%d"), *Settings.Host, Settings.Port);
	BIO_set_conn_hostname(Bio, TCHAR_TO_UTF8(*Address));

	// Blocking connect and handshake. No timeout beyond the OS TCP one, which is
	// fine while the server is on localhost; a dead remote host stalls this
	// thread for ~20s, not the game thread.
	if (BIO_do_connect(Bio) <= 0)
	{
		OutError = FString::Printf(TEXT("cannot reach field server at %s: %s"),
			*Address, *LastOpenSslError());
		return false;
	}

	// Non-blocking from here so one thread can both read and write without
	// either starving the other.
	BIO_socket_nbio(BIO_get_fd(Bio, nullptr), 1);

	UE_LOG(LogHHVField, Display, TEXT("connected to %s (%s, %s)"), *Address,
		UTF8_TO_TCHAR(SSL_get_version(Ssl)), UTF8_TO_TCHAR(SSL_get_cipher(Ssl)));
	return true;
}

void FHHVFieldConnection::CloseTls()
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

bool FHHVFieldConnection::FlushOutbound()
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

bool FHHVFieldConnection::ReadInbound(FString& OutError)
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
			OutError = TEXT("server closed the connection");
			return false;
		}

		OutError = TEXT("recv failed: ") + LastOpenSslError();
		return false;
	}
}

void FHHVFieldConnection::ParseAccumulated()
{
	int32 Offset = 0;
	while (RecvAccum.Num() - Offset >= 4)
	{
		const uint8* Header = RecvAccum.GetData() + Offset;
		const uint32 Size = static_cast<uint32>(Header[0]) | (static_cast<uint32>(Header[1]) << 8) |
			(static_cast<uint32>(Header[2]) << 16) | (static_cast<uint32>(Header[3]) << 24);

		if (Size == 0 || Size > MaxFrameBytes)
		{
			UE_LOG(LogHHVField, Warning, TEXT("frame length %u out of range, dropping stream"), Size);
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

void FHHVFieldConnection::DispatchFrame(const uint8* Data, int32 Size)
{
	// The server is not a trust boundary the way a client is, but a truncated or
	// corrupt frame still reaches GetRoot as raw offsets. Verify first.
	flatbuffers::Verifier Verifier(Data, static_cast<size_t>(Size));
	if (!HeavenField::VerifyEnvelopeBuffer(Verifier))
	{
		UE_LOG(LogHHVField, Warning, TEXT("malformed field frame (%d bytes)"), Size);
		return;
	}

	const HeavenField::Envelope* Envelope = HeavenField::GetEnvelope(Data);

	// Verifier 는 payload_type 만 있고 payload 오프셋이 없는 프레임을 통과시킨다
	// (VerifyTable(nullptr) 이 true 다). 그대로 두면 아래 payload_as_* 가 nullptr 을
	// 돌려주고 바로 역참조해 죽는다.
	if (Envelope == nullptr || Envelope->payload() == nullptr)
	{
		UE_LOG(LogHHVField, Warning, TEXT("field frame carried no payload (%d bytes)"), Size);
		return;
	}

	FHHVFieldEventData Event;
	switch (Envelope->payload_type())
	{
	case HeavenField::Payload::EnterAck:
	{
		const HeavenField::EnterAck* Ack = Envelope->payload_as_EnterAck();
		Event.Type = EHHVFieldEvent::EnterAck;
		Event.EntityId = Ack->entity_id();
		Event.X = Ack->x();
		Event.Y = Ack->y();
		Event.Facing = Ack->facing();
		break;
	}

	case HeavenField::Payload::Correction:
	{
		const HeavenField::Correction* Correction = Envelope->payload_as_Correction();
		Event.Type = EHHVFieldEvent::Correction;
		Event.Sequence = Correction->sequence();
		Event.X = Correction->x();
		Event.Y = Correction->y();
		Event.Facing = Correction->facing();
		break;
	}

	case HeavenField::Payload::Snapshot:
	{
		const HeavenField::Snapshot* Snapshot = Envelope->payload_as_Snapshot();
		Event.Type = EHHVFieldEvent::Snapshot;
		ReadEntities(Snapshot->spawned(), Event.Snapshot.Spawned);
		ReadEntities(Snapshot->moved(), Event.Snapshot.Moved);
		if (const flatbuffers::Vector<uint64_t>* Despawned = Snapshot->despawned())
		{
			Event.Snapshot.Despawned.Reserve(static_cast<int32>(Despawned->size()));
			for (const uint64 EntityId : *Despawned)
			{
				Event.Snapshot.Despawned.Add(EntityId);
			}
		}
		break;
	}

	case HeavenField::Payload::PartyState:
	{
		const HeavenField::PartyState* State = Envelope->payload_as_PartyState();
		Event.Type = EHHVFieldEvent::PartyState;
		Event.Party.bOk = State->ok();
		if (const flatbuffers::String* Message = State->message())
		{
			Event.Party.Message = FString(UTF8_TO_TCHAR(Message->c_str()));
		}
		Event.Party.ActiveDex = State->active_dex();
		if (const flatbuffers::Vector<uint16>* Party = State->dex_numbers())
		{
			Event.Party.Party.Reserve(static_cast<int32>(Party->size()));
			for (const uint16 Dex : *Party)
			{
				Event.Party.Party.Add(Dex);
			}
		}
		if (const flatbuffers::Vector<uint16>* Unlocked = State->unlocked())
		{
			Event.Party.Unlocked.Reserve(static_cast<int32>(Unlocked->size()));
			for (const uint16 Dex : *Unlocked)
			{
				Event.Party.Unlocked.Add(Dex);
			}
		}
		break;
	}

	case HeavenField::Payload::PartnerChanged:
	{
		const HeavenField::PartnerChanged* Changed = Envelope->payload_as_PartnerChanged();
		Event.Type = EHHVFieldEvent::PartnerChanged;
		Event.EntityId = Changed->entity_id();
		Event.PartnerDex = Changed->partner_species();
		break;
	}

	case HeavenField::Payload::Notice:
	{
		const HeavenField::Notice* Notice = Envelope->payload_as_Notice();
		Event.Type = EHHVFieldEvent::Notice;
		if (const flatbuffers::String* Text = Notice->text())
		{
			Event.Text = FString(UTF8_TO_TCHAR(Text->c_str()));
		}
		break;
	}

	default:
		// Enter and Move are client-to-server. The server never sends them.
		UE_LOG(LogHHVField, Warning, TEXT("unexpected payload %u from field server"),
			static_cast<uint32>(Envelope->payload_type()));
		return;
	}

	Inbound.Enqueue(MoveTemp(Event));
}

void FHHVFieldConnection::PushDisconnect(const FString& Reason)
{
	// Run() has several exits and the tail runs after every one of them. Only
	// the first reason is worth reporting -- the rest are consequences.
	if (bDisconnectPushed.AtomicSet(true))
	{
		return;
	}

	FHHVFieldEventData Event;
	Event.Type = EHHVFieldEvent::Disconnected;
	Event.Text = Reason;
	Inbound.Enqueue(MoveTemp(Event));
}

void FHHVFieldConnection::Poll()
{
	FHHVFieldEventData Event;
	while (Inbound.Dequeue(Event))
	{
		switch (Event.Type)
		{
		case EHHVFieldEvent::EnterAck:
			bInField = true;
			if (OnEnterAck)
			{
				OnEnterAck(Event.EntityId, Event.X, Event.Y, Event.Facing);
			}
			break;

		case EHHVFieldEvent::Correction:
			if (OnCorrection)
			{
				OnCorrection(Event.Sequence, Event.X, Event.Y, Event.Facing);
			}
			break;

		case EHHVFieldEvent::Snapshot:
			if (OnSnapshot)
			{
				OnSnapshot(Event.Snapshot);
			}
			break;

		case EHHVFieldEvent::Notice:
			UE_LOG(LogHHVField, Warning, TEXT("server notice: %s"), *Event.Text);
			if (OnNotice)
			{
				OnNotice(Event.Text);
			}
			break;

		case EHHVFieldEvent::PartyState:
			if (OnPartyState)
			{
				OnPartyState(Event.Party);
			}
			break;

		case EHHVFieldEvent::PartnerChanged:
			if (OnPartnerChanged)
			{
				OnPartnerChanged(Event.EntityId, Event.PartnerDex);
			}
			break;

		case EHHVFieldEvent::Disconnected:
			bInField = false;
			UE_LOG(LogHHVField, Warning, TEXT("disconnected: %s"), *Event.Text);
			if (OnDisconnected)
			{
				OnDisconnected(Event.Text);
			}
			break;
		}
	}
}
