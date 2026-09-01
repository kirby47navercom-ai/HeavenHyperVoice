#pragma once

#include "CoreMinimal.h"
#include "CharacterSelection/UECharacterSlotSaveGame.h"
#include "Engine/GameInstance.h"

#include "Containers/Ticker.h"

#include "../Net/HHVLoginConnection.h"

#include <memory>

#include "UEGameInstance.generated.h"

class UUECharacterSlotSaveGame;
class UUEPokemonSpeciesData;
class UUEPokemonSpeciesCatalog;

/** 서버 요청 하나가 끝났다. bOk 가 false 면 Message 가 사용자에게 보여줄 사유다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUEServerResultSignature, bool, bOk, const FString&, Message);

/** 캐릭터 목록이 바뀌었다. 로비가 이걸 받고 다시 그린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUECharacterListChangedSignature);

/** 캐릭터를 골라 티켓을 받았다. 이제 필드로 넘어갈 수 있다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUEEnterReadySignature, const FString&, Nickname);

/** 로컬 세션과 캐릭터 슬롯 상태를 레벨 이동 사이에 유지하는 게임 인스턴스다. */
UCLASS(Config = Game, DefaultConfig)
class HEAVENHYPERVOICE_API UUEGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	
	UUEGameInstance(const FObjectInitializer& ObjectInitializer);

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintCallable, Category = "HHV|Customization")
	void SetPendingHHVAppearance(const FUEHHVAppearance& NewAppearance);

	UFUNCTION(BlueprintPure, Category = "HHV|Customization")
	bool GetPendingHHVAppearance(FUEHHVAppearance& OutAppearance) const;

	UFUNCTION(BlueprintCallable, Category = "HHV|Customization")
	void ClearPendingHHVAppearance();

	UFUNCTION(BlueprintPure, Category = "HHV|Character Selection")
	int32 GetCharacterSlotCount() const { return CharacterSlotCount; }

	UFUNCTION(BlueprintPure, Category = "HHV|Character Selection")
	bool GetCharacterSlot(int32 SlotIndex, FUECharacterSlotData& OutSlot) const;

	UFUNCTION(BlueprintPure, Category = "HHV|Character Selection")
	bool IsCharacterSlotOccupied(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "HHV|Character Selection")
	bool SelectCharacterSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "HHV|Character Selection")
	bool SaveAppearanceToSelectedSlot(const FUEHHVAppearance& Appearance);

	// 커마 완료 시 이름, 외형, 스타팅 포켓몬을 한 번에 저장한다.
	UFUNCTION(BlueprintCallable, Category = "HHV|Character Selection")
	bool SaveCharacterCreationToSelectedSlot(
		const FString& CharacterName,
		const FUEHHVAppearance& Appearance,
		UUEPokemonSpeciesData* PartnerSpecies);

	UFUNCTION(BlueprintCallable, Category = "HHV|Character Selection")
	bool DeleteCharacterSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "HHV|Character Selection")
	bool LoadSelectedSlotAppearance();

	UFUNCTION(BlueprintPure, Category = "HHV|Character Selection")
	int32 GetSelectedCharacterSlotIndex() const;

	UFUNCTION(BlueprintPure, Category = "HHV|Character Selection")
	UUEPokemonSpeciesData* GetSelectedPartnerSpecies() const;

	// 도감번호를 종족 에셋으로 바꾸는 표. ini 의 SpeciesCatalog 가 원본이다.
	// 필드 파티 화면처럼 WBP 없이 뜨는 화면이 빌려 쓴다.
	UFUNCTION(BlueprintPure, Category = "HHV|Pokemon")
	UUEPokemonSpeciesCatalog* GetPartySpeciesCatalog() const { return GetSpeciesCatalog(); }

	// 이름 단계에서 받은 값을 최종 스타팅 선택까지 레벨 이동 사이에 보관한다.
	UFUNCTION(BlueprintCallable, Category = "HHV|Character Creation")
	void SetPendingCharacterName(const FString& CharacterName);

	UFUNCTION(BlueprintPure, Category = "HHV|Character Creation")
	bool GetPendingCharacterName(FString& OutCharacterName) const;

	UFUNCTION(BlueprintCallable, Category = "HHV|Character Creation")
	void ClearPendingCharacterCreation();

	// --- 로그인 서버 ---
	//
	// 연결은 로그인 요청부터 캐릭터 선택까지 열려 있다. 화면(타이틀/로그인/로비)이
	// 바뀌어도 살아 있어야 하므로 위젯이 아니라 여기서 소유한다.
	//
	// 주의: LoginServer 의 --select-timeout(기본 120초)이 이 연결의 상한이다.
	// 로비에서 그보다 오래 머물면 서버가 끊고, OnServerDisconnected 가 온다.

	UFUNCTION(BlueprintCallable, Category = "HHV|Server")
	void ConnectAndLogin(const FString& UserId, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "HHV|Server")
	void ConnectAndRegister(const FString& UserId, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "HHV|Server")
	void DisconnectFromServer();

	UFUNCTION(BlueprintPure, Category = "HHV|Server")
	bool IsLoggedInToServer() const;

	/** 응답을 기다리는 중이다. 버튼을 잠가 두는 데 쓴다. */
	UFUNCTION(BlueprintPure, Category = "HHV|Server")
	bool IsServerRequestPending() const;

	/**
	 * 커마에서 만든 캐릭터를 서버에 만든다. 성공하면 목록이 갱신된다.
	 *
	 * 외형은 여기서 한 번만 저장된다. 만든 뒤에 바꾸는 경로는 없다 —
	 * 프로토콜에 수정 메시지를 두지 않았다.
	 */
	UFUNCTION(BlueprintCallable, Category = "HHV|Server")
	void RequestCreateCharacter(const FString& Nickname,
		UUEPokemonSpeciesData* PartnerSpecies,
		const FUEHHVAppearance& Appearance);

	/** ConfirmNickname 은 사용자가 다시 입력한 값이어야 한다. 서버가 대조한다. */
	UFUNCTION(BlueprintCallable, Category = "HHV|Server")
	void RequestDeleteCharacter(int32 SlotIndex, const FString& ConfirmNickname);

	UFUNCTION(BlueprintCallable, Category = "HHV|Server")
	void RequestSelectCharacter(int32 SlotIndex);

	/** 티켓을 받은 뒤에만 채워진다. 필드 접속에 그대로 쓴다. */
	bool GetFieldEndpoint(FString& OutHost, int32& OutPort, TArray<uint8>& OutTicket) const;

	/** 티켓을 받은 뒤에만 채워진다. 플레이 HUD의 채팅 연결에 쓴다. */
	bool GetChatEndpoint(FString& OutHost, int32& OutPort, TArray<uint8>& OutTicket) const;

	UPROPERTY(BlueprintAssignable, Category = "HHV|Server")
	FUEServerResultSignature OnLoginCompleted;

	UPROPERTY(BlueprintAssignable, Category = "HHV|Server")
	FUEServerResultSignature OnRegisterCompleted;

	UPROPERTY(BlueprintAssignable, Category = "HHV|Server")
	FUEServerResultSignature OnCharacterChangeCompleted;

	UPROPERTY(BlueprintAssignable, Category = "HHV|Server")
	FUECharacterListChangedSignature OnCharacterListChanged;

	UPROPERTY(BlueprintAssignable, Category = "HHV|Server")
	FUEEnterReadySignature OnEnterReady;

	UPROPERTY(BlueprintAssignable, Category = "HHV|Server")
	FUEServerResultSignature OnServerDisconnected;

	// 서버를 붙이기 전까지 로그인 뒤 화면 이동에만 사용하는 로컬 세션이다.
	UFUNCTION(BlueprintCallable, Category = "HHV|Local Session")
	void SetLocalSession(const FString& UserId, const FString& Nickname);

	UFUNCTION(BlueprintCallable, Category = "HHV|Local Session")
	void ClearLocalSession();

	UFUNCTION(BlueprintPure, Category = "HHV|Local Session")
	bool HasLocalSession() const { return bHasLocalSession; }

	UFUNCTION(BlueprintPure, Category = "HHV|Local Session")
	FString GetLocalSessionUserId() const { return LocalSessionUserId; }

	UFUNCTION(BlueprintPure, Category = "HHV|Local Session")
	FString GetLocalSessionNickname() const { return LocalSessionNickname; }

	// 로그인 전에 고른 서버 주소를 이후 레벨에서도 사용할 수 있게 보관한다.
	// 비어 있지 않으면 ConnectAndLogin 이 LoginServerHost 대신 이 값을 쓴다.
	UFUNCTION(BlueprintCallable, Category = "HHV|Server")
	void SetServerAddress(const FString& ServerAddress);

	UFUNCTION(BlueprintPure, Category = "HHV|Server")
	FString GetServerAddress() const { return SelectedServerAddress; }

	// 서버 연결을 편다. GameInstance 는 Tick 이 없어서 엔진 티커로 돈다.
	void PollServer();

private:
	void LoadCharacterSlots();
	bool SaveCharacterSlots() const;
	bool IsValidCharacterSlot(int32 SlotIndex) const;

	// 연결을 열고 콜백을 건다. 이미 열려 있으면 아무것도 하지 않는다.
	bool EnsureServerConnection();
	void BindServerCallbacks();

	// 서버가 준 캐릭터 목록을 로비가 읽는 슬롯 모양으로 옮긴다. 로비 위젯은
	// 로컬 SaveGame 때와 같은 API 를 계속 쓴다.
	void ApplyServerCharacters(const TArray<FHHVCharacterSummary>& Characters);

	UUEPokemonSpeciesCatalog* GetSpeciesCatalog() const;

	static constexpr int32 CharacterSlotCount = 3;

	UPROPERTY(Config)
	int32 CharacterSlotUserIndex = 0;

	UPROPERTY(Config)
	FString CharacterSlotSaveName;

	// 커마 레벨에서 선택한 상태를 다음 레벨 로딩 뒤에도 읽을 수 있게 GameInstance에 잠깐 보관한다.
	UPROPERTY(Transient)
	FUEHHVAppearance PendingHHVAppearance;

	UPROPERTY(Transient)
	bool bHasPendingHHVAppearance = false;

	UPROPERTY(Transient)
	FString PendingCharacterName;

	UPROPERTY(Transient)
	TObjectPtr<UUECharacterSlotSaveGame> CharacterSlotSave = nullptr;

	UPROPERTY(Transient)
	FString LocalSessionUserId;

	UPROPERTY(Transient)
	FString LocalSessionNickname;

	UPROPERTY(Transient)
	bool bHasLocalSession = false;

	// --- 서버 세션 ---

	// 접속 화면에서 주소를 입력하지 않았을 때 쓰는 기본값. ini 에서 온다.
	UPROPERTY(EditDefaultsOnly, Config, Category = "HHV|Server")
	FString LoginServerHost = TEXT("127.0.0.1");

	UPROPERTY(EditDefaultsOnly, Config, Category = "HHV|Server")
	int32 LoginServerPort = 9100;

	/** 종족 id 를 데이터 에셋으로 바꿀 때 쓴다. BP 기본값에서 지정한다. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "HHV|Server")
	TSoftObjectPtr<UUEPokemonSpeciesCatalog> SpeciesCatalog;

	std::unique_ptr<FHHVLoginConnection> LoginConnection;

	// 월드가 아니라 엔진 티커에 물린다. 레벨이 갈려도 살아남는다.
	FTSTicker::FDelegateHandle ServerPollHandle;

	// 서버가 준 목록. 로비에 보이는 것의 원본이다.
	TArray<FHHVCharacterSummary> ServerCharacters;

	// 캐릭터를 고르고 받은 것. 필드/채팅 접속에 쓴다.
	TArray<FHHVServiceEndpoint> ServiceEndpoints;

	UPROPERTY(Transient)
	int32 ServerMaxSlots = CharacterSlotCount;

	// 접속 화면에서 입력한 주소. 비어 있으면 LoginServerHost 를 쓴다.
	UPROPERTY(Transient)
	FString SelectedServerAddress;
};
