#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Net/HHVFieldConnection.h"
#include "Templates/SubclassOf.h"

#include <memory>

#include "UEFieldServerBridgeComponent.generated.h"

class AUEPlayerCharacter;
class AUEPokemonCharacter;
class UTexture2D;
class UUEFieldRemotePlayerSyncComponent;
class UUEFieldPartnerSyncComponent;
class UUEFieldPartyWidget;

/** 서버가 알려준 파티 상태. 전부 도감번호다. */
USTRUCT(BlueprintType)
struct FUEFieldPartyState
{
	GENERATED_BODY()

	// 순서가 슬롯 번호다. 최대 세 마리.
	UPROPERTY(BlueprintReadOnly, Category = "Field Server|Party")
	TArray<int32> Party;

	// 지금 꺼내 놓은 한 마리. 0 이면 아무도 안 꺼냈다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Server|Party")
	int32 ActiveDex = 0;

	// 해금한 종족 전부. 파티 후보다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Server|Party")
	TArray<int32> Unlocked;

	// 마지막 요청에 대한 서버 답변. 화면에 그대로 띄운다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Server|Party")
	bool bOk = false;

	UPROPERTY(BlueprintReadOnly, Category = "Field Server|Party")
	FString Message;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUEOnFieldPartyStateChanged);

class UUEFieldWildPokemonSyncComponent;
class UUEPlayerMovementSyncComponent;

USTRUCT(BlueprintType)
struct FUEFieldPokemonPartyEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Pokemon")
	int32 PokemonInstanceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Pokemon")
	FName SpeciesId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Pokemon")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Pokemon")
	TObjectPtr<UTexture2D> ProfileIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Pokemon", meta = (ClampMin = "1"))
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Pokemon", meta = (ClampMin = "0.0"))
	float CurrentHP = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Pokemon", meta = (ClampMin = "1.0"))
	float MaxHP = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Pokemon")
	bool bSelected = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Pokemon")
	bool bCanSummon = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUEFieldPokemonPartyChangedSignature);

UCLASS(ClassGroup = (Custom), Config = Game, DefaultConfig, meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEFieldServerBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEFieldServerBridgeComponent();
	virtual void BeginDestroy() override;

	void AttachToPlayer(AUEPlayerCharacter* PlayerCharacter);
	void DetachFromPlayer();

	UFUNCTION(BlueprintPure, Category = "Field Server")
	bool IsExternalFieldServerConfigured() const;

	UFUNCTION(BlueprintCallable, Category = "Field Server|Pokemon")
	bool SendPokemonAttackRequest(int32 AttackSlot);

	UFUNCTION(BlueprintPure, Category = "Field Server|Pokemon")
	TArray<FUEFieldPokemonPartyEntry> GetPokemonPartyEntries() const { return PokemonPartyEntries; }

	const TArray<FUEFieldPokemonPartyEntry>& GetCachedPokemonPartyEntries() const { return PokemonPartyEntries; }
	void ReplacePokemonPartyEntriesFromServer(TArray<FUEFieldPokemonPartyEntry> NewEntries);

	UPROPERTY(BlueprintAssignable, Category = "Field Server|Pokemon")
	FUEFieldPokemonPartyChangedSignature OnPokemonPartyChanged;

	// 파티가 바뀔 때마다(입장 직후 포함) 브로드캐스트한다. 파티 화면이 붙는다.
	UPROPERTY(BlueprintAssignable, Category = "Field Server|Party")
	FUEOnFieldPartyStateChanged OnPartyStateChanged;

	/** 서버가 마지막으로 알려준 파티 상태. 화면이 열릴 때 이걸로 그린다. */
	UFUNCTION(BlueprintPure, Category = "Field Server|Party")
	const FUEFieldPartyState& GetPartyState() const { return PartyState; }

	/** 파티와 꺼낼 한 마리를 서버에 보낸다. 응답은 OnPartyStateChanged 로 온다. */
	UFUNCTION(BlueprintCallable, Category = "Field Server|Party")
	bool SendSetParty(const TArray<int32>& DexNumbers, int32 ActiveDex);

	/** 파티 화면을 켜고 끈다. 포켓몬 꺼내기 키에 걸려 있다. */
	UFUNCTION(BlueprintCallable, Category = "Field Server|Party")
	void TogglePartyWidget();

	/**
	 * 다음에 붙을 곳. 0 이면 필드, 그 외에는 그 번호의 인스턴스다.
	 *
	 * 들어가고 나오는 것 자체는 UUEFieldClientSubsystem 이 주도한다 — 레벨이
	 * 갈리면서 이 컴포넌트가 통째로 사라지기 때문에, 목적지를 기억하는 것은
	 * 레벨을 넘어 사는 쪽이어야 한다. 여기서는 받아 적기만 한다.
	 *
	 * AttachToPlayer **전에** 불러야 한다. 붙는 그 자리에서 접속을 시작한다.
	 */
	void SetConnectionTarget(int32 InstanceType) { TargetInstanceType = InstanceType; }

	UFUNCTION(BlueprintPure, Category = "Field Server|Instance")
	bool IsInInstance() const { return bInInstance; }

	/** 배정받은 방 번호. 필드에서는 0 이다. 로그와 디버깅용. */
	UFUNCTION(BlueprintPure, Category = "Field Server|Instance")
	int32 GetRoomId() const { return static_cast<int32>(CurrentRoomId); }

	// 파티 화면. 없으면 키를 눌러도 아무 일도 없다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Field Server|Party")
	TSubclassOf<UUEFieldPartyWidget> PartyWidgetClass;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	void HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Field Server")
	FString FieldServerHost;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Field Server", meta = (ClampMin = "1", ClampMax = "65535"))
	int32 FieldServerPort = 0;

	// 티켓이 없는 개발 접속에서만 쓴다. 티켓이 있으면 로그인 서버가 알려준
	// 주소를 쓰므로 이 값은 무시된다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Field Server", meta = (ClampMin = "1", ClampMax = "65535"))
	int32 InstanceServerPort = 9300;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Field Server")
	FString DevCharacterName;

	static constexpr int64 DefaultDevCharacterId = 9001;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Field Server", meta = (ClampMin = "1"))
	int64 DevCharacterId = DefaultDevCharacterId;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Field Server", meta = (ClampMin = "0.01"))
	float SendIntervalSeconds = 0.05f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Field Server")
	float WorldOriginOffset = 0.0f;

	// 야생 포켓몬으로 스폰할 클래스. 네이티브 AUEPokemonCharacter 는 메시도
	// 종족 카탈로그도 없어서 스폰해 봐야 보이지 않는다 — 둘 다 BP_Pokemon 이
	// 들고 있다. Config 라서 DefaultGame.ini 에서 지정한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Field Server|Wild Pokemon")
	TSubclassOf<AUEPokemonCharacter> WildPokemonClass;

private:
	void ResolveSyncComponents();

	// Service 는 "field" 또는 "instance". 티켓이 있으면 로그인 서버가 준 주소로,
	// 없으면 설정에 박힌 개발용 주소로 붙는다.
	void StartConnection(const FString& Service, uint32 InstanceType);

	// SetConnectionTarget 이 정해 둔 곳으로 붙는다.
	void StartFieldConnection()
	{
		StartConnection(TargetInstanceType > 0 ? TEXT("instance") : TEXT("field"),
			static_cast<uint32>(FMath::Max(TargetInstanceType, 0)));
	}
	void StopFieldConnection();
	void DestroyPresentationActors();
	void HandleFieldEnterAck(uint64 EntityId, float ServerX, float ServerY, float Facing,
		uint32 RoomId, float OriginOffset);
	void HandleFieldCorrection(uint32 Sequence, float ServerX, float ServerY, float Facing);
	void HandleFieldSnapshot(const FHHVFieldSnapshot& Snapshot);
	void HandleFieldDisconnected(const FString& Reason);
	void HandleFieldPartyState(const FHHVFieldPartyState& State);
	void HandleFieldPartnerChanged(uint64 EntityId, uint16 PartnerDex);
	FVector MakeEntityLocation(float ServerX, float ServerY) const;
	float ToServerAxis(double UnrealAxis) const { return static_cast<float>(UnrealAxis) + WorldOriginOffset; }
	double ToUnrealAxis(float ServerAxis) const { return static_cast<double>(ServerAxis - WorldOriginOffset); }
	AUEPlayerCharacter* GetPlayerCharacter() const;

	TWeakObjectPtr<AUEPlayerCharacter> CachedPlayerCharacter;
	TWeakObjectPtr<UUEPlayerMovementSyncComponent> MovementSyncComponent;
	TWeakObjectPtr<UUEFieldWildPokemonSyncComponent> WildPokemonSyncComponent;
	TWeakObjectPtr<UUEFieldPartnerSyncComponent> PartnerSyncComponent;

	// 서버가 준 마지막 파티 상태. 화면은 이것만 읽는다 — 자기 상태를 따로
	// 들고 있으면 거절당한 변경이 화면에만 남는다.
	UPROPERTY(Transient)
	FUEFieldPartyState PartyState;

	// 내 엔티티 번호. PartnerChanged 가 나에게 온 것인지 가리는 데 쓴다.
	uint64 LocalEntityId = 0;

	// 지금 인스턴스에 붙어 있는가. 위 TargetInstanceType 이 "가려는 곳" 이라면
	// 이쪽은 "실제로 붙은 곳" 이다.
	bool bInInstance = false;
	uint32 CurrentRoomId = 0;

	// 서브시스템이 알려준 목적지. 0 이면 필드.
	int32 TargetInstanceType = 0;

	UPROPERTY(Transient)
	TObjectPtr<UUEFieldPartyWidget> PartyWidget = nullptr;
	TWeakObjectPtr<UUEFieldRemotePlayerSyncComponent> RemotePlayerSyncComponent;

	std::unique_ptr<FHHVFieldConnection> FieldConnection;
	TArray<FUEFieldPokemonPartyEntry> PokemonPartyEntries;
	int32 SnapshotsLogged = 0;
	float TimeSinceLastSend = 0.0f;
};
