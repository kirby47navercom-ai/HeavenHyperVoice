#pragma once

#include "CoreMinimal.h"
#include "CharacterSelection/UECharacterSlotSaveGame.h"
#include "Engine/GameInstance.h"
#include "UEGameInstance.generated.h"

class UUECharacterSlotSaveGame;
class UUEPokemonSpeciesData;

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

	// 이름 단계에서 받은 값을 최종 스타팅 선택까지 레벨 이동 사이에 보관한다.
	UFUNCTION(BlueprintCallable, Category = "HHV|Character Creation")
	void SetPendingCharacterName(const FString& CharacterName);

	UFUNCTION(BlueprintPure, Category = "HHV|Character Creation")
	bool GetPendingCharacterName(FString& OutCharacterName) const;

	UFUNCTION(BlueprintCallable, Category = "HHV|Character Creation")
	void ClearPendingCharacterCreation();

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
	UFUNCTION(BlueprintCallable, Category = "HHV|Server")
	void SetServerAddress(const FString& ServerAddress);

	UFUNCTION(BlueprintPure, Category = "HHV|Server")
	FString GetServerAddress() const { return SelectedServerAddress; }

private:
	void LoadCharacterSlots();
	bool SaveCharacterSlots() const;
	bool IsValidCharacterSlot(int32 SlotIndex) const;

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

	UPROPERTY(Transient)
	FString SelectedServerAddress;
	
};
