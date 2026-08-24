// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterSelection/UECharacterSlotSaveGame.h"
#include "Engine/GameInstance.h"
#include "UEGameInstance.generated.h"

class UUECharacterSlotSaveGame;

/**
 * 
 */
UCLASS()
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

	UFUNCTION(BlueprintCallable, Category = "HHV|Character Selection")
	bool LoadSelectedSlotAppearance();

	UFUNCTION(BlueprintPure, Category = "HHV|Character Selection")
	int32 GetSelectedCharacterSlotIndex() const;

private:
	void LoadCharacterSlots();
	bool SaveCharacterSlots() const;
	bool IsValidCharacterSlot(int32 SlotIndex) const;

	static constexpr int32 CharacterSlotCount = 3;
	static constexpr int32 CharacterSlotUserIndex = 0;
	static const FString CharacterSlotSaveName;

	// 커마 레벨에서 선택한 상태를 다음 레벨 로딩 뒤에도 읽을 수 있게 GameInstance에 잠깐 보관한다.
	UPROPERTY(Transient)
	FUEHHVAppearance PendingHHVAppearance;

	UPROPERTY(Transient)
	bool bHasPendingHHVAppearance = false;

	UPROPERTY(Transient)
	TObjectPtr<UUECharacterSlotSaveGame> CharacterSlotSave = nullptr;
	
};
