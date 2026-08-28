#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Net/HHVFieldConnection.h"

#include <memory>

#include "UEFieldServerBridgeComponent.generated.h"

class AUEPlayerCharacter;
class AUEPokemonCharacter;
class UTexture2D;
class UUEFieldRemotePlayerSyncComponent;
class UUEFieldPartnerSyncComponent;
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
	bool SendPokemonToggleRequest();

	UFUNCTION(BlueprintCallable, Category = "Field Server|Pokemon")
	bool SendPokemonAttackRequest(int32 AttackSlot);

	UFUNCTION(BlueprintPure, Category = "Field Server|Pokemon")
	TArray<FUEFieldPokemonPartyEntry> GetPokemonPartyEntries() const { return PokemonPartyEntries; }

	const TArray<FUEFieldPokemonPartyEntry>& GetCachedPokemonPartyEntries() const { return PokemonPartyEntries; }
	void ReplacePokemonPartyEntriesFromServer(TArray<FUEFieldPokemonPartyEntry> NewEntries);

	UPROPERTY(BlueprintAssignable, Category = "Field Server|Pokemon")
	FUEFieldPokemonPartyChangedSignature OnPokemonPartyChanged;

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
	void StartFieldConnection();
	void StopFieldConnection();
	void DestroyPresentationActors();
	void HandleFieldEnterAck(uint64 EntityId, float ServerX, float ServerY, float Facing);
	void HandleFieldCorrection(uint32 Sequence, float ServerX, float ServerY, float Facing);
	void HandleFieldSnapshot(const FHHVFieldSnapshot& Snapshot);
	void HandleFieldDisconnected(const FString& Reason);
	FVector MakeEntityLocation(float ServerX, float ServerY) const;
	float ToServerAxis(double UnrealAxis) const { return static_cast<float>(UnrealAxis) + WorldOriginOffset; }
	double ToUnrealAxis(float ServerAxis) const { return static_cast<double>(ServerAxis - WorldOriginOffset); }
	AUEPlayerCharacter* GetPlayerCharacter() const;

	TWeakObjectPtr<AUEPlayerCharacter> CachedPlayerCharacter;
	TWeakObjectPtr<UUEPlayerMovementSyncComponent> MovementSyncComponent;
	TWeakObjectPtr<UUEFieldWildPokemonSyncComponent> WildPokemonSyncComponent;
	TWeakObjectPtr<UUEFieldPartnerSyncComponent> PartnerSyncComponent;
	TWeakObjectPtr<UUEFieldRemotePlayerSyncComponent> RemotePlayerSyncComponent;

	std::unique_ptr<FHHVFieldConnection> FieldConnection;
	TArray<FUEFieldPokemonPartyEntry> PokemonPartyEntries;
	int32 SnapshotsLogged = 0;
	float TimeSinceLastSend = 0.0f;
};
