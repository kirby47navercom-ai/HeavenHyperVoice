#include "HHVChatConnection.h"

#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"

#define UI OPENSSL_UI

THIRD_PARTY_INCLUDES_START
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
THIRD_PARTY_INCLUDES_END

#undef UI

THIRD_PARTY_INCLUDES_START
#include "flatbuffers/flatbuffers.h"
#include "Generated/chat_generated.h"
THIRD_PARTY_INCLUDES_END

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogHHVChat, Log, All);

namespace HHVChatConnectionPrivate
{
	constexpr int32 MaxFrameBytes = 64 * 1024;
	constexpr int32 MaxChatTextBytes = 1024;
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

FHHVChatConnection::~FHHVChatConnection()
{
	Shutdown();
}

void FHHVChatConnection::Start(const FHHVChatSettings& InSettings)
{
	if (Thread != nullptr)
	{
		return;
	}

	Settings = InSettings;
	bStopRequested = false;
	bConnected = false;
	bDisconnectPushed = false;

	// Hello가 어떤 채팅보다 반드시 먼저 서버에 도착하도록 스레드를 띄우기 전에 넣는다.
	flatbuffers::FlatBufferBuilder Builder(512);
	const auto Ticket = Builder.CreateVector(Settings.Ticket.GetData(), Settings.Ticket.Num());
	const auto Hello = HeavenChat::CreateHello(Builder, Ticket);
	Builder.Finish(HeavenChat::CreateEnvelope(
		Builder, HeavenChat::Payload::Hello, Hello.Union()));
	Outbound.Enqueue(HHVChatConnectionPrivate::FrameOf(Builder));

	Thread = FRunnableThread::Create(this, TEXT("HHVChatConnection"), 0, TPri_BelowNormal);
}

void FHHVChatConnection::Shutdown()
{
	bStopRequested = true;
	bConnected = false;
	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

void FHHVChatConnection::Stop()
{
	bStopRequested = true;
}

bool FHHVChatConnection::ValidateText(const FString& Text, FString& OutError)
{
	const FString Trimmed = Text.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		OutError = TEXT("메시지를 입력해 주세요");
		return false;
	}

	const FTCHARToUTF8 Utf8(*Trimmed);
	if (Utf8.Length() > HHVChatConnectionPrivate::MaxChatTextBytes)
	{
		OutError = TEXT("메시지는 UTF-8 기준 1024바이트까지 보낼 수 있습니다");
		return false;
	}

	OutError.Reset();
	return true;
}

bool FHHVChatConnection::SendSay(const FString& Text, FString& OutError)
{
	if (!ValidateText(Text, OutError))
	{
		return false;
	}
	if (bStopRequested || !bConnected)
	{
		OutError = TEXT("채팅 서버에 연결되어 있지 않습니다");
		return false;
	}

	const FString Trimmed = Text.TrimStartAndEnd();
	flatbuffers::FlatBufferBuilder Builder(256);
	const auto Body = Builder.CreateString(TCHAR_TO_UTF8(*Trimmed));
	const auto Say = HeavenChat::CreateSay(Builder, Body);
	Builder.Finish(HeavenChat::CreateEnvelope(Builder, HeavenChat::Payload::Say, Say.Union()));
	Outbound.Enqueue(HHVChatConnectionPrivate::FrameOf(Builder));
	return true;
}

uint32 FHHVChatConnection::Run()
{
	FString Error;
	if (!ConnectAndHandshake(Error))
	{
		PushDisconnect(Error);
		CloseTls();
		return 0;
	}
	bConnected = true;

	while (!bStopRequested)
	{
		if (!FlushOutbound())
		{
			PushDisconnect(TEXT("send failed: ") + HHVChatConnectionPrivate::LastOpenSslError());
			break;
		}
		if (!ReadInbound(Error))
		{
			PushDisconnect(Error);
			break;
		}
		FPlatformProcess::Sleep(HHVChatConnectionPrivate::IdleSleepSeconds);
	}

	bConnected = false;
	CloseTls();
	PushDisconnect(TEXT("closed"));
	return 0;
}

bool FHHVChatConnection::ConnectAndHandshake(FString& OutError)
{
	Ctx = SSL_CTX_new(TLS_client_method());
	if (Ctx == nullptr)
	{
		OutError = TEXT("SSL_CTX_new failed: ") + HHVChatConnectionPrivate::LastOpenSslError();
		return false;
	}

	SSL_CTX_set_min_proto_version(Ctx, TLS1_2_VERSION);
	// 개발 서버가 로그인/필드와 같은 자체 서명 인증서를 사용한다.
	SSL_CTX_set_verify(Ctx, SSL_VERIFY_NONE, nullptr);

	Bio = BIO_new_ssl_connect(Ctx);
	if (Bio == nullptr)
	{
		OutError = TEXT("BIO_new_ssl_connect failed: ") + HHVChatConnectionPrivate::LastOpenSslError();
		return false;
	}

	BIO_get_ssl(Bio, &Ssl);
	if (Ssl == nullptr)
	{
		OutError = TEXT("BIO_get_ssl failed: ") + HHVChatConnectionPrivate::LastOpenSslError();
		return false;
	}

	SSL_set_mode(Ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	const FString Address = FString::Printf(TEXT("%s:%d"), *Settings.Host, Settings.Port);
	BIO_set_conn_hostname(Bio, TCHAR_TO_UTF8(*Address));
	if (BIO_do_connect(Bio) <= 0)
	{
		OutError = FString::Printf(TEXT("채팅 서버 %s에 연결할 수 없습니다: %s"),
			*Address, *HHVChatConnectionPrivate::LastOpenSslError());
		return false;
	}

	BIO_socket_nbio(BIO_get_fd(Bio, nullptr), 1);
	UE_LOG(LogHHVChat, Display, TEXT("connected to %s"), *Address);
	return true;
}

void FHHVChatConnection::CloseTls()
{
	if (Bio != nullptr)
	{
		BIO_free_all(Bio);
		Bio = nullptr;
		Ssl = nullptr;
	}
	if (Ctx != nullptr)
	{
		SSL_CTX_free(Ctx);
		Ctx = nullptr;
	}
}

bool FHHVChatConnection::FlushOutbound()
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
			return true;
		}
		return false;
	}
}

bool FHHVChatConnection::ReadInbound(FString& OutError)
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
			OutError = TEXT("채팅 서버가 연결을 종료했습니다");
			return false;
		}

		OutError = TEXT("recv failed: ") + HHVChatConnectionPrivate::LastOpenSslError();
		return false;
	}
}

void FHHVChatConnection::ParseAccumulated()
{
	int32 Offset = 0;
	while (RecvAccum.Num() - Offset >= 4)
	{
		const uint8* Header = RecvAccum.GetData() + Offset;
		const uint32 Size = static_cast<uint32>(Header[0]) |
			(static_cast<uint32>(Header[1]) << 8) |
			(static_cast<uint32>(Header[2]) << 16) |
			(static_cast<uint32>(Header[3]) << 24);

		if (Size == 0 || Size > HHVChatConnectionPrivate::MaxFrameBytes)
		{
			RecvAccum.Reset();
			PushDisconnect(TEXT("잘못된 채팅 프레임입니다"));
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

void FHHVChatConnection::DispatchFrame(const uint8* Data, int32 Size)
{
	flatbuffers::Verifier Verifier(Data, static_cast<size_t>(Size));
	if (!HeavenChat::VerifyEnvelopeBuffer(Verifier))
	{
		UE_LOG(LogHHVChat, Warning, TEXT("malformed chat frame (%d bytes)"), Size);
		return;
	}

	const HeavenChat::Envelope* Envelope = HeavenChat::GetEnvelope(Data);
	if (Envelope == nullptr || Envelope->payload() == nullptr)
	{
		return;
	}

	FHHVChatEventData Event;
	switch (Envelope->payload_type())
	{
	case HeavenChat::Payload::Notice:
	{
		Event.Type = EHHVChatEvent::Notice;
		const HeavenChat::Notice* Notice = Envelope->payload_as_Notice();
		if (Notice->text())
		{
			Event.Text = UTF8_TO_TCHAR(Notice->text()->c_str());
		}
		break;
	}
	case HeavenChat::Payload::Chat:
	{
		Event.Type = EHHVChatEvent::Message;
		const HeavenChat::Chat* Chat = Envelope->payload_as_Chat();
		if (Chat->nickname())
		{
			Event.Nickname = UTF8_TO_TCHAR(Chat->nickname()->c_str());
		}
		if (Chat->text())
		{
			Event.Text = UTF8_TO_TCHAR(Chat->text()->c_str());
		}
		break;
	}
	default:
		return;
	}

	Inbound.Enqueue(MoveTemp(Event));
}

void FHHVChatConnection::PushDisconnect(const FString& Reason)
{
	if (bDisconnectPushed.AtomicSet(true))
	{
		return;
	}

	FHHVChatEventData Event;
	Event.Type = EHHVChatEvent::Disconnected;
	Event.Text = Reason;
	Inbound.Enqueue(MoveTemp(Event));
}

void FHHVChatConnection::Poll()
{
	FHHVChatEventData Event;
	while (Inbound.Dequeue(Event))
	{
		switch (Event.Type)
		{
		case EHHVChatEvent::Notice:
			if (OnNotice)
			{
				OnNotice(Event.Text);
			}
			break;
		case EHHVChatEvent::Message:
			if (OnMessage)
			{
				OnMessage(Event.Nickname, Event.Text);
			}
			break;
		case EHHVChatEvent::Disconnected:
			if (OnDisconnected)
			{
				OnDisconnected(Event.Text);
			}
			break;
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVChatTextValidationTest,
	"HeavenHyperVoice.Chat.TextValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVChatTextValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Error;
	TestFalse(TEXT("blank text is rejected"), FHHVChatConnection::ValidateText(TEXT("  "), Error));
	TestTrue(TEXT("1024 ASCII bytes are accepted"),
		FHHVChatConnection::ValidateText(FString::ChrN(1024, TEXT('a')), Error));
	TestFalse(TEXT("multibyte UTF-8 is measured in bytes"),
		FHHVChatConnection::ValidateText(FString::ChrN(342, TEXT('가')), Error));
	return true;
}
#endif
