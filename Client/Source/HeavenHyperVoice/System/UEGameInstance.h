// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../CharacterCustomization/Palworld/Data/UEPalworldCustomizationTypes.h"
#include "Engine/GameInstance.h"
#include "UEGameInstance.generated.h"

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

	UFUNCTION(BlueprintCallable, Category = "Palworld|Customization")
	void SetPendingPalworldAppearance(const FUEPalworldAppearance& NewAppearance);

	UFUNCTION(BlueprintPure, Category = "Palworld|Customization")
	bool GetPendingPalworldAppearance(FUEPalworldAppearance& OutAppearance) const;

	UFUNCTION(BlueprintCallable, Category = "Palworld|Customization")
	void ClearPendingPalworldAppearance();

private:
	// 커마 레벨에서 선택한 상태를 다음 레벨 로딩 뒤에도 읽을 수 있게 GameInstance에 잠깐 보관한다.
	UPROPERTY(Transient)
	FUEPalworldAppearance PendingPalworldAppearance;

	UPROPERTY(Transient)
	bool bHasPendingPalworldAppearance = false;
	
};
