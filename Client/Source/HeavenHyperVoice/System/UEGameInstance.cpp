// Fill out your copyright notice in the Description page of Project Settings.


#include "UEGameInstance.h"
#include "../Data/UEProjectAssets.h"
#include "../System/UEAssetManager.h"
#include "../UI/Loading/HHVLoadingScreenSettings.h"
#include "CharacterSelection/UECharacterSlotSaveGame.h"
#include "../Pokemon/UEPokemonSpeciesData.h"
#include "../Pokemon/UEPokemonSpeciesCatalog.h"

#include "AssetCompilingManager.h"
#include "Blueprint/UserWidget.h"
#include "ContentStreaming.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "MoviePlayer.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "YANG2_CLIENT_AUTHORITY_ONLY local session changes must never compile on main."
#endif

namespace
{
	// 로그인 연결은 요청/응답이 드물다. 매 프레임 볼 이유가 없고, 이 간격이면
	// 버튼을 누른 뒤 반응이 늦다고 느껴지지 않는다.
	constexpr float ServerPollIntervalSeconds = 0.05f;
}

UUEGameInstance::UUEGameInstance(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UUEGameInstance::Init()
{
	Super::Init();

	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(
		this, &ThisClass::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &ThisClass::HandlePostLoadMap);

	// 화면 에셋은 첫 이동 직전에 동기 로드하지 않도록 게임 시작 때 준비한다.
	if (const UUEProjectAssets* Assets = UUEProjectAssetSettings::GetProjectAssets())
	{
		Assets->LoadingScreenWidgetClass.LoadSynchronous();
	}

	UUEAssetManager::Initialize();
	LoadCharacterSlots();
}

void UUEGameInstance::Shutdown()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (PendingTravelHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PendingTravelHandle);
		PendingTravelHandle.Reset();
	}
	if (LoadingFinishHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LoadingFinishHandle);
		LoadingFinishHandle.Reset();
	}
	FCoreDelegates::OnSamplingInput.Remove(LoadingMoviePollHandle);
	LoadingMoviePollHandle.Reset();
	HideLoadingScreen();

	// 워커 스레드를 조인한다. 콜백이 매달린 참조가 되기 전에 먼저 정리한다.
	DisconnectFromServer();

	Super::Shutdown();
}

void UUEGameInstance::HandlePreLoadMap(const FString& MapName)
{
	(void)MapName;

	// 게임을 켜고 첫 맵(타이틀)을 여는 동안에는 로딩 화면을 띄우지 않는다. 이때는 로딩 화면의
	// 배경 텍스처도 아직 준비 전이라 까만 화면 위에 막대만 보였다.
	if (!bFirstMapLoadStarted)
	{
		bFirstMapLoadStarted = true;
		return;
	}

	ShowLoadingScreen();
	bLoadingMapStarted = true;
}

void UUEGameInstance::ShowLoadingScreen()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}
	if (LoadingScreenSlateWidget.IsValid())
	{
		return;
	}

	const UUEProjectAssets* Assets = UUEProjectAssetSettings::GetProjectAssets();
	UClass* WidgetClass = Assets ? Assets->LoadingScreenWidgetClass.LoadSynchronous() : nullptr;
	if (!WidgetClass)
	{
		return;
	}

	if (!LoadingScreenWidget)
	{
		LoadingScreenWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	}
	if (!LoadingScreenWidget)
	{
		return;
	}

	LoadingScreenSlateWidget = LoadingScreenWidget->TakeWidget();
	LoadingScreenStartedAtSeconds = FPlatformTime::Seconds();
	bPostLoadAssetsReady = false;

	if (IsMoviePlayerEnabled())
	{
		FLoadingScreenAttributes Attributes;
		Attributes.WidgetLoadingScreen = LoadingScreenSlateWidget;
		Attributes.bAutoCompleteWhenLoadingCompletes = false;
		Attributes.bWaitForManualStop = true;
		Attributes.bMoviesAreSkippable = false;
		GetMoviePlayer()->SetupLoadingScreen(Attributes);
		GetMoviePlayer()->PlayMovie();

		// bWaitForManualStop 이라 무비 플레이어는 맵 로드가 끝난 직후(PostLoadMap) 자기 루프에서
		// 게임 스레드를 붙잡고 StopMovie 를 기다린다. 그동안 코어 티커(FinishLoadingScreen)는 돌지
		// 않아 입력이 들어올 때까지 로딩 화면이 닫히지 않았다. 그 루프 안에서도 매번 불리는
		// OnSamplingInput 에서 준비가 끝났는지 보고 직접 멈춘다.
		FCoreDelegates::OnSamplingInput.Remove(LoadingMoviePollHandle);
		LoadingMoviePollHandle = FCoreDelegates::OnSamplingInput.AddUObject(
			this, &ThisClass::PollLoadingScreenWhileMovieWaits);
	}
	else if (GEngine && GEngine->GameViewport && !bLoadingScreenInViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(LoadingScreenSlateWidget.ToSharedRef(), 10000);
		bLoadingScreenInViewport = true;
	}
}

void UUEGameInstance::OpenLevelWithLoadingScreen(TSoftObjectPtr<UWorld> TargetLevel,
	const bool bAbsolute, const FString& Options)
{
	if (TargetLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("HHV: OpenLevelWithLoadingScreen needs a World Asset in the editor."));
		return;
	}

	if (PendingTravelHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PendingTravelHandle);
	}

	PendingTravelLevel = TargetLevel;
	PendingTravelOptions = Options;
	bPendingTravelAbsolute = bAbsolute;
	ShowLoadingScreen();

	// PIE에서도 로딩 화면이 실제로 그려진 뒤 OpenLevel의 동기 로딩을 시작한다.
	PendingTravelHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::BeginPendingLevelTravel), 0.05f);
}

bool UUEGameInstance::BeginPendingLevelTravel(float DeltaSeconds)
{
	(void)DeltaSeconds;
	const TSoftObjectPtr<UWorld> TargetLevel = PendingTravelLevel;
	const FString Options = MoveTemp(PendingTravelOptions);
	const bool bAbsolute = bPendingTravelAbsolute;

	PendingTravelHandle.Reset();
	PendingTravelLevel.Reset();
	PendingTravelOptions.Reset();

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, TargetLevel, bAbsolute, Options);
	return false;
}

void UUEGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadingScreenSlateWidget.IsValid())
	{
		return;
	}

	LoadedWorldForLoadingScreen = LoadedWorld;
	if (LoadingFinishHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LoadingFinishHandle);
	}
	LoadingFinishHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::FinishLoadingScreen));
}

bool UUEGameInstance::FinishLoadingScreen(float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (!IsLoadedWorldReadyToReveal())
	{
		return true;
	}

	HideLoadingScreen();
	LoadingFinishHandle.Reset();
	return false;
}

bool UUEGameInstance::IsLoadedWorldReadyToReveal()
{
	const UHHVLoadingScreenSettings* Settings = GetDefault<UHHVLoadingScreenSettings>();

	if (!bPostLoadAssetsReady)
	{
		// 무비 플레이어 대기 중에는 HandlePostLoadMap 보다 먼저 불릴 수 있어 지금 월드로 대신한다.
		UWorld* LoadedWorld = LoadedWorldForLoadingScreen.Get();
		if (!LoadedWorld)
		{
			LoadedWorld = GetWorld();
		}
		if (LoadedWorld)
		{
			LoadedWorld->BlockTillLevelStreamingCompleted();
		}

		FlushAsyncLoading();
		FAssetCompilingManager::Get().FinishAllCompilation();

		if (Settings->bWaitForAssetStreaming)
		{
			IStreamingManager::Get().NotifyLevelChange();
			if (IStreamingManager::Get().StreamAllResources() > 0)
			{
				return false;
			}
		}

		bPostLoadAssetsReady = true;
	}

	return FPlatformTime::Seconds() - LoadingScreenStartedAtSeconds >= Settings->MinimumDisplaySeconds;
}

void UUEGameInstance::PollLoadingScreenWhileMovieWaits()
{
	// OnSamplingInput 은 평소 프레임에도 불린다. 맵 로드가 끝나 새 월드가 BeginPlay 한 뒤에만 본다.
	// (OpenLevelWithLoadingScreen 이 로딩 화면을 먼저 띄우고 실제 이동은 다음 틱에 시작한다.)
	// IsMovieCurrentlyPlaying 은 보지 않는다. 무비 플레이어는 대기 루프에 들어가며 로딩 스레드를
	// 먼저 정리해서, 루프 안에서는 재생 중이 아니라고 답한다.
	const UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay() || !LoadingScreenSlateWidget.IsValid() || !IsMoviePlayerEnabled())
	{
		return;
	}
	if (!bLoadingMapStarted || !IsLoadedWorldReadyToReveal())
	{
		return;
	}

	FCoreDelegates::OnSamplingInput.Remove(LoadingMoviePollHandle);
	LoadingMoviePollHandle.Reset();
	// 여기서는 WaitForMovieToFinish 를 다시 부르면 안 된다 (이미 그 루프 안이다). 멈춤 표시만 하면
	// 루프가 끝나고, 남은 정리는 FinishLoadingScreen 이 한다.
	GetMoviePlayer()->StopMovie();
}

void UUEGameInstance::HideLoadingScreen()
{
	if (IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		GetMoviePlayer()->StopMovie();
		GetMoviePlayer()->WaitForMovieToFinish();
	}

	if (bLoadingScreenInViewport && LoadingScreenSlateWidget.IsValid()
		&& GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(LoadingScreenSlateWidget.ToSharedRef());
	}

	bLoadingScreenInViewport = false;
	LoadedWorldForLoadingScreen.Reset();
	LoadingScreenSlateWidget.Reset();
	LoadingScreenWidget = nullptr;
	bLoadingMapStarted = false;
}

// ---------------------------------------------------------------- 로그인 서버

UUEPokemonSpeciesCatalog* UUEGameInstance::GetSpeciesCatalog() const
{
	if (!SpeciesCatalog.IsNull()) return SpeciesCatalog.LoadSynchronous();
	const UUEProjectAssets* Assets = UUEProjectAssetSettings::GetProjectAssets();
	return Assets ? Assets->SpeciesCatalog.LoadSynchronous() : nullptr;
}

bool UUEGameInstance::EnsureServerConnection()
{
	// 객체가 있다고 연결이 살아 있는 것이 아니다. 서버가 끊으면 워커는 끝나지만
	// unique_ptr 은 그대로 남는다. 그걸 재사용하면 요청이 아무도 읽지 않는 큐에
	// 쌓이고, 응답이 안 오니 버튼이 잠긴 채로 멈춘다.
	if (LoginConnection && LoginConnection->IsClosed())
	{
		UE_LOG(LogTemp, Display, TEXT("HHV: login connection was closed, reconnecting"));
		DisconnectFromServer();
	}

	if (LoginConnection)
	{
		return true;
	}

	ServiceEndpoints.Reset();
	LoginConnection = std::make_unique<FHHVLoginConnection>();
	BindServerCallbacks();

	// 접속 화면에서 입력한 주소가 있으면 그걸 쓴다. 없으면 ini 기본값이다.
	FHHVLoginSettings ConnectionSettings;
	ConnectionSettings.Host = SelectedServerAddress.IsEmpty() ? LoginServerHost
	                                                          : SelectedServerAddress;
	ConnectionSettings.Port = LoginServerPort;
	LoginConnection->Start(ConnectionSettings);

	// GameInstance 에는 Tick 이 없다. 월드 타이머 대신 엔진 티커를 쓴다 —
	// 커마에서 로비로 갈 때 OpenLevel 로 월드가 갈리고, 월드 타이머는 그때
	// 같이 사라진다. 그러면 응답을 아무도 꺼내지 않아 화면이 멈춘다.
	if (!ServerPollHandle.IsValid())
	{
		ServerPollHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float) {
				PollServer();
				return true;
			}),
			ServerPollIntervalSeconds);
	}

	UE_LOG(LogTemp, Display, TEXT("HHV: login server %s:%d"),
		*ConnectionSettings.Host, ConnectionSettings.Port);
	return true;
}

void UUEGameInstance::BindServerCallbacks()
{
	// 연결은 이 GameInstance 가 소유하고 Shutdown 에서 먼저 정리하므로 this 캡처가
	// 안전하다. Poll() 은 타이머에서만 돌아서 콜백은 전부 게임 스레드에 떨어진다.
	LoginConnection->OnLoginResponse =
		[this](bool bOk, const FString& Message, const TArray<FHHVCharacterSummary>& Characters,
			int32 MaxSlots)
	{
		if (bOk)
		{
			ServerMaxSlots = MaxSlots > 0 ? MaxSlots : CharacterSlotCount;
			ApplyServerCharacters(Characters);
			OnCharacterListChanged.Broadcast();
		}
		OnLoginCompleted.Broadcast(bOk, Message);
	};

	LoginConnection->OnRegisterResponse = [this](bool bOk, const FString& Message)
	{
		// 가입 응답 뒤 서버가 연결을 끊는다. 다음 로그인은 새로 연결한다.
		OnRegisterCompleted.Broadcast(bOk, Message);
	};

	LoginConnection->OnCheckNicknameResponse = [this](bool bOk, const FString& Message)
	{
		OnNicknameChecked.Broadcast(bOk, Message);
	};

	LoginConnection->OnCharacterList =
		[this](bool bOk, const FString& Message, const TArray<FHHVCharacterSummary>& Characters)
	{
		// 성공이든 실패든 최신 목록이 실려 온다. 실패 시 빈 목록이면 덮지 않는다.
		if (bOk || Characters.Num() > 0)
		{
			ApplyServerCharacters(Characters);
			OnCharacterListChanged.Broadcast();
		}
		OnCharacterChangeCompleted.Broadcast(bOk, Message);
	};

	LoginConnection->OnSelectResponse =
		[this](bool bOk, const FString& Message, const TArray<FHHVServiceEndpoint>& Endpoints,
			const FString& Nickname)
	{
		if (!bOk)
		{
			OnCharacterChangeCompleted.Broadcast(false, Message);
			return;
		}

		ServiceEndpoints = Endpoints;
		SetLocalSession(LocalSessionUserId, Nickname);
		OnEnterReady.Broadcast(Nickname);
	};

	LoginConnection->OnDisconnected = [this](const FString& Reason)
	{
		// 서버는 마지막 응답을 보낸 뒤에 연결을 닫는다 (LoginHandler::finish).
		// 가입도, 로그인 거절도, 캐릭터 선택도 전부 그렇다. 그 끊김까지 실패로
		// 알리면 방금 띄운 응답 문구를 끊김 사유가 덮어써서, 가입에 성공하고도
		// 화면에는 "recv failed: ..." 가 남는다.
		//
		// 기다리던 요청이 남아 있을 때만 사고다. 응답이 왔으면 그 응답이 이미
		// 결과를 알렸다.
		if (LoginConnection && LoginConnection->IsBusy())
		{
			OnServerDisconnected.Broadcast(false, Reason);
		}
	};
}

void UUEGameInstance::PollServer()
{
	if (LoginConnection)
	{
		LoginConnection->Poll();
	}
}

void UUEGameInstance::ConnectAndLogin(const FString& UserId, const FString& Password)
{
	// YANG2_CLIENT_AUTHORITY_ONLY: 서버 접속 없이 기존 로컬 캐릭터 슬롯으로 들어간다.
	const FString LocalUserId = UserId.TrimStartAndEnd();
	if (LocalUserId.IsEmpty() || Password.IsEmpty())
	{
		OnLoginCompleted.Broadcast(false, TEXT("아이디와 비밀번호를 입력해 주세요"));
		return;
	}
	DisconnectFromServer();
	SetLocalSession(LocalUserId, FString());
	OnCharacterListChanged.Broadcast();
	OnLoginCompleted.Broadcast(true, TEXT("Yang2 로컬 로그인"));
	return;

	EnsureServerConnection();
	LocalSessionUserId = UserId;
	LoginConnection->SendLogin(UserId, Password);
}

void UUEGameInstance::ConnectAndRegister(const FString& UserId, const FString& Password)
{
	// YANG2_CLIENT_AUTHORITY_ONLY: 계정 서버 없이 UI 흐름만 완료한다.
	if (UserId.TrimStartAndEnd().IsEmpty() || Password.IsEmpty())
	{
		OnRegisterCompleted.Broadcast(false, TEXT("아이디와 비밀번호를 입력해 주세요"));
		return;
	}
	OnRegisterCompleted.Broadcast(true, TEXT("Yang2에서는 별도 가입 없이 로그인할 수 있습니다"));
	return;

	EnsureServerConnection();
	LoginConnection->SendRegister(UserId, Password);
}

void UUEGameInstance::DisconnectFromServer()
{
	if (ServerPollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ServerPollHandle);
		ServerPollHandle.Reset();
	}
	LoginConnection.reset();
}

bool UUEGameInstance::IsLoggedInToServer() const
{
	// YANG2_CLIENT_AUTHORITY_ONLY
	return HasLocalSession();
}

bool UUEGameInstance::IsServerRequestPending() const
{
	// YANG2_CLIENT_AUTHORITY_ONLY: 모든 로컬 응답은 같은 게임 스레드에서 즉시 끝난다.
	return false;
}

void UUEGameInstance::RequestCheckNickname(const FString& Nickname)
{
	// YANG2_CLIENT_AUTHORITY_ONLY
	const FString Candidate = Nickname.TrimStartAndEnd();
	if (!HasLocalSession() || Candidate.Len() < 2 || Candidate.Len() > 32)
	{
		OnNicknameChecked.Broadcast(false, TEXT("닉네임은 2자 이상 32자 이하로 입력해 주세요"));
		return;
	}
	for (int32 SlotIndex = 0; SlotIndex < CharacterSlotCount; ++SlotIndex)
	{
		FUECharacterSlotData Slot;
		if (GetCharacterSlot(SlotIndex, Slot) && IsCharacterSlotOccupied(SlotIndex) &&
			Slot.CharacterName.Equals(Candidate, ESearchCase::IgnoreCase))
		{
			OnNicknameChecked.Broadcast(false, TEXT("이미 사용 중인 닉네임입니다"));
			return;
		}
	}
	OnNicknameChecked.Broadcast(true, TEXT("사용할 수 있는 닉네임입니다"));
	return;

	// 로그인 뒤 캐릭터 선택 단계에서만 답할 수 있는 질문이다. 연결이 없으면
	// 새로 붙어 봐야 인증이 없어 거절당하므로 여기서 끊는다.
	if (!IsLoggedInToServer())
	{
		OnNicknameChecked.Broadcast(false, TEXT("서버에 연결되어 있지 않습니다"));
		return;
	}

	LoginConnection->SendCheckNickname(Nickname);
}

void UUEGameInstance::RequestCreateCharacter(const FString& Nickname,
	UUEPokemonSpeciesData* PartnerSpecies, const FUEHHVAppearance& Appearance)
{
	// YANG2_CLIENT_AUTHORITY_ONLY: 서버 DB 대신 기존 SaveGame 슬롯에 저장한다.
	if (!HasLocalSession())
	{
		OnCharacterChangeCompleted.Broadcast(false, TEXT("로컬 로그인이 필요합니다"));
		return;
	}
	const FString Candidate = Nickname.TrimStartAndEnd();
	for (int32 SlotIndex = 0; SlotIndex < CharacterSlotCount; ++SlotIndex)
	{
		FUECharacterSlotData Slot;
		if (GetCharacterSlot(SlotIndex, Slot) && IsCharacterSlotOccupied(SlotIndex) &&
			Slot.CharacterName.Equals(Candidate, ESearchCase::IgnoreCase))
		{
			OnCharacterChangeCompleted.Broadcast(false, TEXT("이미 사용 중인 닉네임입니다"));
			return;
		}
	}
	if (!SaveCharacterCreationToSelectedSlot(Candidate, Appearance, PartnerSpecies))
	{
		OnCharacterChangeCompleted.Broadcast(false, TEXT("로컬 캐릭터 저장에 실패했습니다"));
		return;
	}
	ResetYang2GameplayState();
	OnCharacterListChanged.Broadcast();
	OnCharacterChangeCompleted.Broadcast(true, TEXT("Yang2 로컬 캐릭터를 만들었습니다"));
	return;

	if (!IsLoggedInToServer())
	{
		OnCharacterChangeCompleted.Broadcast(false, TEXT("로그인이 필요합니다"));
		return;
	}

	// 서버는 도감번호로 종족을 받는다. 번호가 안 채워진 에셋이면 0 이 되고, 0 은
	// "파트너 없이 시작" 이라 서버가 거절하지 않는다 — 그러면 로비에서 미완성
	// 슬롯으로 숨겨지므로 여기서 막는다.
	int32 DexNumber = 0;
	if (UUEPokemonSpeciesCatalog* Catalog = GetSpeciesCatalog())
	{
		DexNumber = Catalog->FindDexNumber(PartnerSpecies);
	}
	if (PartnerSpecies != nullptr && DexNumber == 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("HHV: starter has no DexNumber; refusing to create. Fill it on the species data asset."));
		OnCharacterChangeCompleted.Broadcast(false, TEXT("스타팅 포켓몬을 찾을 수 없습니다"));
		return;
	}

	LoginConnection->SendCreateCharacter(Nickname, static_cast<uint16>(DexNumber), Appearance);
}

void UUEGameInstance::RequestDeleteCharacter(int32 SlotIndex, const FString& ConfirmNickname)
{
	// YANG2_CLIENT_AUTHORITY_ONLY
	FUECharacterSlotData Slot;
	if (!GetCharacterSlot(SlotIndex, Slot) || !IsCharacterSlotOccupied(SlotIndex) ||
		!Slot.CharacterName.Equals(ConfirmNickname.TrimStartAndEnd(), ESearchCase::CaseSensitive) ||
		!DeleteCharacterSlot(SlotIndex))
	{
		OnCharacterChangeCompleted.Broadcast(false, TEXT("캐릭터를 삭제하지 못했습니다"));
		return;
	}
	ResetYang2GameplayState();
	OnCharacterListChanged.Broadcast();
	OnCharacterChangeCompleted.Broadcast(true, TEXT("캐릭터를 삭제했습니다"));
	return;

	if (!IsLoggedInToServer() || !ServerCharacters.IsValidIndex(SlotIndex))
	{
		OnCharacterChangeCompleted.Broadcast(false, TEXT("캐릭터를 찾을 수 없습니다"));
		return;
	}
	LoginConnection->SendDeleteCharacter(ServerCharacters[SlotIndex].Id, ConfirmNickname);
}

void UUEGameInstance::RequestSelectCharacter(int32 SlotIndex)
{
	// YANG2_CLIENT_AUTHORITY_ONLY: 티켓 발급을 기다리지 않고 필드 입장 신호를 낸다.
	FUECharacterSlotData Slot;
	if (!HasLocalSession() || !GetCharacterSlot(SlotIndex, Slot) ||
		!IsCharacterSlotOccupied(SlotIndex) || !SelectCharacterSlot(SlotIndex))
	{
		OnCharacterChangeCompleted.Broadcast(false, TEXT("캐릭터를 찾을 수 없습니다"));
		return;
	}
	SetLocalSession(LocalSessionUserId, Slot.CharacterName);
	PrepareYang2GameplayState();
	OnEnterReady.Broadcast(Slot.CharacterName);
	return;

	if (!IsLoggedInToServer() || !ServerCharacters.IsValidIndex(SlotIndex))
	{
		OnCharacterChangeCompleted.Broadcast(false, TEXT("캐릭터를 찾을 수 없습니다"));
		return;
	}
	LoginConnection->SendSelectCharacter(ServerCharacters[SlotIndex].Id);
}

bool UUEGameInstance::GetServiceEndpoint(const FString& Service, FString& OutHost,
	int32& OutPort, TArray<uint8>& OutTicket) const
{
	for (const FHHVServiceEndpoint& Endpoint : ServiceEndpoints)
	{
		if (Endpoint.Service == Service)
		{
			OutHost = Endpoint.Host;
			OutPort = Endpoint.Port;
			OutTicket = Endpoint.Ticket;
			return true;
		}
	}
	return false;
}

bool UUEGameInstance::GetChatEndpoint(FString& OutHost, int32& OutPort,
	TArray<uint8>& OutTicket) const
{
	for (const FHHVServiceEndpoint& Endpoint : ServiceEndpoints)
	{
		if (Endpoint.Service == TEXT("chat"))
		{
			OutHost = Endpoint.Host;
			OutPort = Endpoint.Port;
			OutTicket = Endpoint.Ticket;
			return true;
		}
	}
	return false;
}

void UUEGameInstance::ApplyServerCharacters(const TArray<FHHVCharacterSummary>& Characters)
{
	ServerCharacters = Characters;

	// 로비 위젯은 로컬 SaveGame 때와 같은 슬롯 API 를 계속 쓴다. 서버 목록을
	// 그 모양으로 옮겨 담아 위젯을 건드리지 않는다.
	if (!CharacterSlotSave)
	{
		return;
	}

	CharacterSlotSave->Slots.Reset();
	CharacterSlotSave->Slots.SetNum(FMath::Max(ServerMaxSlots, CharacterSlotCount));

	UUEPokemonSpeciesCatalog* Catalog = GetSpeciesCatalog();
	if (!Catalog)
	{
		// 캐릭터 자체는 뜬다. 파트너만 비어 보이므로 원인을 로그로 남긴다.
		UE_LOG(LogTemp, Error,
			TEXT("HHV: SpeciesCatalog is not assigned. Lobby characters will show no partner."));
	}

	for (int32 Index = 0; Index < ServerCharacters.Num(); ++Index)
	{
		if (!CharacterSlotSave->Slots.IsValidIndex(Index))
		{
			break;
		}

		const FHHVCharacterSummary& Source = ServerCharacters[Index];
		FUECharacterSlotData& Slot = CharacterSlotSave->Slots[Index];
		Slot.bOccupied = true;
		Slot.CharacterName = Source.Nickname;
		Slot.Appearance = Source.Appearance;
		Slot.PartnerSpecies = nullptr;

		if (Source.bHasPartner && Catalog)
		{
			// 배열 위치가 아니라 도감번호로 찾는다. 카탈로그에 종족을 끼워 넣어도
			// 이미 저장된 파트너가 다른 종족으로 바뀌지 않는다.
			Slot.PartnerSpecies = Catalog->FindByDex(static_cast<int32>(Source.Partner.DexNumber));
			if (Slot.PartnerSpecies.IsNull())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("HHV: dex %d is not in the catalog; character '%s' shows without a ")
					TEXT("partner. Fill DexNumber on the species data asset."),
					Source.Partner.DexNumber, *Source.Nickname);
			}
		}

		UE_LOG(LogTemp, Verbose,
			TEXT("HHV: slot %d = '%s' (id %llu, partner=%s, species=%d)"),
			Index, *Source.Nickname, Source.Id,
			Source.bHasPartner ? TEXT("yes") : TEXT("no"),
			Source.bHasPartner ? Source.Partner.SpeciesId : 0);
	}

	// 서버 목록은 로컬에 저장하지 않는다. 다음 로그인에 다시 받는다.
}

void UUEGameInstance::SetPendingHHVAppearance(const FUEHHVAppearance& NewAppearance)
{
	PendingHHVAppearance = NewAppearance;
	bHasPendingHHVAppearance = true;
}

bool UUEGameInstance::GetPendingHHVAppearance(FUEHHVAppearance& OutAppearance) const
{
	if (!bHasPendingHHVAppearance)
	{
		return false;
	}

	OutAppearance = PendingHHVAppearance;
	return true;
}

void UUEGameInstance::ClearPendingHHVAppearance()
{
	bHasPendingHHVAppearance = false;
	PendingHHVAppearance = FUEHHVAppearance();
}

bool UUEGameInstance::GetCharacterSlot(int32 SlotIndex, FUECharacterSlotData& OutSlot) const
{
	if (!IsValidCharacterSlot(SlotIndex))
	{
		return false;
	}

	OutSlot = CharacterSlotSave->Slots[SlotIndex];
	return true;
}

bool UUEGameInstance::IsCharacterSlotOccupied(int32 SlotIndex) const
{
	if (!IsValidCharacterSlot(SlotIndex))
	{
		return false;
	}

	const FUECharacterSlotData& Slot = CharacterSlotSave->Slots[SlotIndex];

	// 파트너는 보지 않는다. 파트너 없는 캐릭터도 정식 캐릭터다 -- 서버는
	// speciesId 0 으로 만드는 것("파트너 없이 시작")을 받아 주고 방생도 된다.
	// 파트너를 조건에 넣었더니 그런 캐릭터가 로비에서 통째로 사라졌다.
	return Slot.bOccupied && !Slot.CharacterName.TrimStartAndEnd().IsEmpty();
}

bool UUEGameInstance::SelectCharacterSlot(int32 SlotIndex)
{
	if (!IsValidCharacterSlot(SlotIndex))
	{
		return false;
	}

	const int32 PreviousSelectedSlotIndex = CharacterSlotSave->SelectedSlotIndex;
	CharacterSlotSave->SelectedSlotIndex = SlotIndex;
	if (!SaveCharacterSlots())
	{
		// 저장 실패 시 메모리의 선택 상태도 이전 값으로 되돌린다.
		CharacterSlotSave->SelectedSlotIndex = PreviousSelectedSlotIndex;
		return false;
	}

	if (IsCharacterSlotOccupied(SlotIndex))
	{
		SetPendingHHVAppearance(CharacterSlotSave->Slots[SlotIndex].Appearance);
	}
	else
	{
		ClearPendingCharacterCreation();
	}
	return true;
}

bool UUEGameInstance::SaveAppearanceToSelectedSlot(const FUEHHVAppearance& Appearance)
{
	const int32 SlotIndex = GetSelectedCharacterSlotIndex();
	if (!IsValidCharacterSlot(SlotIndex))
	{
		return false;
	}

	FUECharacterSlotData& Slot = CharacterSlotSave->Slots[SlotIndex];
	const FUECharacterSlotData PreviousSlot = Slot;
	Slot.Appearance = Appearance;
	if (!SaveCharacterSlots())
	{
		// 디스크 저장이 실패하면 아직 확정되지 않은 슬롯 변경을 남기지 않는다.
		Slot = PreviousSlot;
		return false;
	}

	SetPendingHHVAppearance(Appearance);
	return true;
}

bool UUEGameInstance::SaveCharacterCreationToSelectedSlot(
	const FString& CharacterName,
	const FUEHHVAppearance& Appearance,
	UUEPokemonSpeciesData* PartnerSpecies)
{
	const int32 SlotIndex = GetSelectedCharacterSlotIndex();
	if (!IsValidCharacterSlot(SlotIndex) || CharacterName.TrimStartAndEnd().IsEmpty() || !PartnerSpecies)
	{
		return false;
	}

	FUECharacterSlotData& Slot = CharacterSlotSave->Slots[SlotIndex];
	const FUECharacterSlotData PreviousSlot = Slot;
	Slot.bOccupied = true;
	Slot.CharacterName = CharacterName.TrimStartAndEnd();
	Slot.Appearance = Appearance;
	Slot.PartnerSpecies = PartnerSpecies;
	if (!SaveCharacterSlots())
	{
		// 실패한 생성 결과는 슬롯에 남기지 않고 같은 입력으로 다시 시도할 수 있게 한다.
		Slot = PreviousSlot;
		return false;
	}

	SetPendingHHVAppearance(Appearance);
	PendingCharacterName.Reset();
	return true;
}

bool UUEGameInstance::DeleteCharacterSlot(int32 SlotIndex)
{
	if (!IsCharacterSlotOccupied(SlotIndex))
	{
		return false;
	}

	const FUECharacterSlotData PreviousSlot = CharacterSlotSave->Slots[SlotIndex];
	const int32 PreviousSelectedSlotIndex = CharacterSlotSave->SelectedSlotIndex;
	CharacterSlotSave->Slots[SlotIndex] = FUECharacterSlotData();
	if (PreviousSelectedSlotIndex == SlotIndex)
	{
		CharacterSlotSave->SelectedSlotIndex = INDEX_NONE;
	}

	if (!SaveCharacterSlots())
	{
		// 삭제 저장이 실패하면 메모리에서도 슬롯을 복구해 재시작 전후 상태를 맞춘다.
		CharacterSlotSave->Slots[SlotIndex] = PreviousSlot;
		CharacterSlotSave->SelectedSlotIndex = PreviousSelectedSlotIndex;
		return false;
	}

	if (PreviousSelectedSlotIndex == SlotIndex)
	{
		ClearPendingCharacterCreation();
	}
	return true;
}

bool UUEGameInstance::LoadSelectedSlotAppearance()
{
	const int32 SlotIndex = GetSelectedCharacterSlotIndex();
	if (!IsCharacterSlotOccupied(SlotIndex))
	{
		return false;
	}

	SetPendingHHVAppearance(CharacterSlotSave->Slots[SlotIndex].Appearance);
	return true;
}

int32 UUEGameInstance::GetSelectedCharacterSlotIndex() const
{
	return CharacterSlotSave ? CharacterSlotSave->SelectedSlotIndex : INDEX_NONE;
}

UUEPokemonSpeciesData* UUEGameInstance::GetSelectedPartnerSpecies() const
{
	FUECharacterSlotData Slot;
	if (!GetCharacterSlot(GetSelectedCharacterSlotIndex(), Slot)
		|| !IsCharacterSlotOccupied(GetSelectedCharacterSlotIndex()))
	{
		return nullptr;
	}

	return Slot.PartnerSpecies.LoadSynchronous();
}

void UUEGameInstance::SetPendingCharacterName(const FString& CharacterName)
{
	PendingCharacterName = CharacterName.TrimStartAndEnd();
}

bool UUEGameInstance::GetPendingCharacterName(FString& OutCharacterName) const
{
	OutCharacterName = PendingCharacterName;
	return !PendingCharacterName.IsEmpty();
}

void UUEGameInstance::ClearPendingCharacterCreation()
{
	PendingCharacterName.Reset();
	ClearPendingHHVAppearance();
}

void UUEGameInstance::ApplyPlayerParty(int64 PartyId, const TArray<FUEPlayerPartyMember>& Members)
{
	PlayerPartyId = PartyId;
	PlayerPartyMembers = Members;
	OnPartyChanged.Broadcast();
}

bool UUEGameInstance::IsPlayerPartyLeader() const
{
	// 명단의 첫 번째가 파티장이다. 계정 번호 대신 닉네임으로 견주는 이유는,
	// 클라이언트가 자기 계정 번호를 알 방법이 없기 때문이다 — 티켓은 불투명하다.
	// 닉네임은 DB 에서 전역 유일하므로 이걸로 충분하다.
	return PlayerPartyMembers.Num() > 0 &&
		PlayerPartyMembers[0].Nickname == LocalSessionNickname;
}

void UUEGameInstance::SetLocalSession(const FString& UserId, const FString& Nickname)
{
	LocalSessionUserId = UserId.TrimStartAndEnd();
	LocalSessionNickname = Nickname.TrimStartAndEnd();
	bHasLocalSession = !LocalSessionUserId.IsEmpty();
}

void UUEGameInstance::ClearLocalSession()
{
	ServiceEndpoints.Reset();
	ServerCharacters.Reset();
	ApplyPlayerParty(0, {});
	ClearPendingCharacterCreation();
	ClearPendingHHVAppearance();
	LocalSessionUserId.Reset();
	LocalSessionNickname.Reset();
	bHasLocalSession = false;
	ResetYang2GameplayState();
}

void UUEGameInstance::PrepareYang2GameplayState()
{
	const int32 SlotIndex = GetSelectedCharacterSlotIndex();
	if (!IsCharacterSlotOccupied(SlotIndex))
	{
		return;
	}
	if (bYang2GameplayStateInitialized && Yang2GameplaySlot == SlotIndex)
	{
		return;
	}

	ResetYang2GameplayState();
	Yang2GameplaySlot = SlotIndex;
	bYang2GameplayStateInitialized = true;
	Yang2PokemonTokens = 20;

	if (const UUEPokemonSpeciesData* Starter = GetSelectedPartnerSpecies())
	{
		const int32 StarterDex = Starter->DexNumber;
		if (StarterDex > 0)
		{
			Yang2UnlockedPokemon.Add(StarterDex);
			Yang2PokemonParty.Add(StarterDex);
			Yang2ActivePokemon = StarterDex;
		}
	}
}

void UUEGameInstance::ResetYang2GameplayState()
{
	Yang2GameplaySlot = INDEX_NONE;
	Yang2UnlockedPokemon.Reset();
	Yang2PokemonParty.Reset();
	Yang2ActivePokemon = 0;
	Yang2PokemonTokens = 20;
	bYang2GameplayStateInitialized = false;
}

bool UUEGameInstance::SetYang2PokemonParty(const TArray<int32>& DexNumbers, int32 ActiveDex,
	FString& OutMessage)
{
	PrepareYang2GameplayState();
	if (DexNumbers.Num() > 3)
	{
		OutMessage = TEXT("파티는 3마리까지입니다");
		return false;
	}
	TSet<int32> Unique;
	for (const int32 Dex : DexNumbers)
	{
		if (Dex <= 0 || Unique.Contains(Dex) || !Yang2UnlockedPokemon.Contains(Dex))
		{
			OutMessage = TEXT("해금하지 않았거나 중복된 포켓몬이 있습니다");
			return false;
		}
		Unique.Add(Dex);
	}
	if (ActiveDex != 0 && !DexNumbers.Contains(ActiveDex))
	{
		OutMessage = TEXT("꺼낼 포켓몬이 파티에 없습니다");
		return false;
	}

	Yang2PokemonParty = DexNumbers;
	Yang2ActivePokemon = ActiveDex;
	OutMessage = TEXT("Yang2 로컬 파티를 저장했습니다");
	return true;
}

bool UUEGameInstance::SpendYang2PokemonToken()
{
	PrepareYang2GameplayState();
	if (Yang2PokemonTokens <= 0)
	{
		return false;
	}
	--Yang2PokemonTokens;
	return true;
}

void UUEGameInstance::GrantYang2PokemonToken()
{
	PrepareYang2GameplayState();
	++Yang2PokemonTokens;
}

bool UUEGameInstance::UnlockYang2Pokemon(int32 DexNumber)
{
	PrepareYang2GameplayState();
	if (DexNumber <= 0 || Yang2UnlockedPokemon.Contains(DexNumber))
	{
		return false;
	}
	Yang2UnlockedPokemon.Add(DexNumber);
	return true;
}

void UUEGameInstance::PrepareReturnToFrontend(bool bLogout)
{
	// 필드 연결과 인증 정보를 비운 뒤 프런트 화면에서 다시 로그인하도록 한다.
	DisconnectFromServer();
	ClearLocalSession();
	bFrontendLoginRequested = !bLogout;
}

bool UUEGameInstance::ConsumeFrontendLoginRequest()
{
	const bool bRequested = bFrontendLoginRequested;
	bFrontendLoginRequested = false;
	return bRequested;
}

void UUEGameInstance::SetServerAddress(const FString& ServerAddress)
{
	SelectedServerAddress = ServerAddress.TrimStartAndEnd();
}

void UUEGameInstance::LoadCharacterSlots()
{
	CharacterSlotSave = Cast<UUECharacterSlotSaveGame>(
		UGameplayStatics::LoadGameFromSlot(CharacterSlotSaveName, CharacterSlotUserIndex));

	if (!CharacterSlotSave)
	{
		CharacterSlotSave = Cast<UUECharacterSlotSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UUECharacterSlotSaveGame::StaticClass()));
	}

	if (!CharacterSlotSave)
	{
		return;
	}

	CharacterSlotSave->Slots.SetNum(CharacterSlotCount);
	if (!IsValidCharacterSlot(CharacterSlotSave->SelectedSlotIndex))
	{
		CharacterSlotSave->SelectedSlotIndex = INDEX_NONE;
	}

	LoadSelectedSlotAppearance();
}

bool UUEGameInstance::SaveCharacterSlots() const
{
	return CharacterSlotSave && UGameplayStatics::SaveGameToSlot(
		CharacterSlotSave,
		CharacterSlotSaveName,
		CharacterSlotUserIndex);
}

bool UUEGameInstance::IsValidCharacterSlot(int32 SlotIndex) const
{
	return CharacterSlotSave && CharacterSlotSave->Slots.IsValidIndex(SlotIndex);
}
