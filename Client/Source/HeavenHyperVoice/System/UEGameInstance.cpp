// Fill out your copyright notice in the Description page of Project Settings.


#include "UEGameInstance.h"
#include "../System/UEAssetManager.h"
#include "CharacterSelection/UECharacterSlotSaveGame.h"
#include "../Pokemon/UEPokemonSpeciesData.h"
#include "../Pokemon/UEPokemonSpeciesCatalog.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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

	UUEAssetManager::Initialize();
	LoadCharacterSlots();
}

void UUEGameInstance::Shutdown()
{
	// 워커 스레드를 조인한다. 콜백이 매달린 참조가 되기 전에 먼저 정리한다.
	DisconnectFromServer();

	Super::Shutdown();
}

// ---------------------------------------------------------------- 로그인 서버

UUEPokemonSpeciesCatalog* UUEGameInstance::GetSpeciesCatalog() const
{
	return SpeciesCatalog.IsNull() ? nullptr : SpeciesCatalog.LoadSynchronous();
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
		// 캐릭터를 고른 뒤에는 서버가 정상적으로 끊는다. 티켓을 이미 받았으면
		// 실패로 알리지 않는다 — 필드로 넘어가는 정상 경로다.
		const bool bExpected = ServiceEndpoints.Num() > 0;
		if (!bExpected)
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
	EnsureServerConnection();
	LocalSessionUserId = UserId;
	LoginConnection->SendLogin(UserId, Password);
}

void UUEGameInstance::ConnectAndRegister(const FString& UserId, const FString& Password)
{
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
	return LoginConnection && LoginConnection->IsAuthenticated();
}

bool UUEGameInstance::IsServerRequestPending() const
{
	return LoginConnection && LoginConnection->IsBusy();
}

void UUEGameInstance::RequestCreateCharacter(const FString& Nickname,
	UUEPokemonSpeciesData* PartnerSpecies, const FUEHHVAppearance& Appearance)
{
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
	if (!IsLoggedInToServer() || !ServerCharacters.IsValidIndex(SlotIndex))
	{
		OnCharacterChangeCompleted.Broadcast(false, TEXT("캐릭터를 찾을 수 없습니다"));
		return;
	}
	LoginConnection->SendDeleteCharacter(ServerCharacters[SlotIndex].Id, ConfirmNickname);
}

void UUEGameInstance::RequestSelectCharacter(int32 SlotIndex)
{
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
		// 로비는 PartnerSpecies 가 채워진 슬롯만 완성 캐릭터로 친다
		// (IsCharacterSlotOccupied). 카탈로그가 없으면 서버가 보낸 캐릭터가
		// 조용히 사라지므로, 원인을 로그로 남긴다.
		UE_LOG(LogTemp, Error,
			TEXT("HHV: SpeciesCatalog is not assigned. Server characters will not appear in the lobby."));
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
					TEXT("HHV: dex %d is not in the catalog; character '%s' will be hidden. ")
					TEXT("Fill DexNumber on the species data asset."),
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
	// 이름·외형·파트너가 모두 확정된 슬롯만 로비의 완성 캐릭터로 취급한다.
	return Slot.bOccupied
		&& !Slot.CharacterName.TrimStartAndEnd().IsEmpty()
		&& !Slot.PartnerSpecies.IsNull();
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

void UUEGameInstance::SetLocalSession(const FString& UserId, const FString& Nickname)
{
	LocalSessionUserId = UserId.TrimStartAndEnd();
	LocalSessionNickname = Nickname.TrimStartAndEnd();
	bHasLocalSession = !LocalSessionUserId.IsEmpty();
}

void UUEGameInstance::ClearLocalSession()
{
	LocalSessionUserId.Reset();
	LocalSessionNickname.Reset();
	bHasLocalSession = false;
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
