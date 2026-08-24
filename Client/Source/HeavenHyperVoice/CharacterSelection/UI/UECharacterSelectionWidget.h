#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UECharacterSelectionWidget.generated.h"

class UButton;
class UTextBlock;
class AGameModeBase;
class UWorld;

/** 디자이너 UMG의 세 캐릭터 슬롯을 저장 데이터와 연결한다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUECharacterSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Character Selection")
	void SelectSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Character Selection")
	void StartSelectedCharacter();

	UFUNCTION(BlueprintCallable, Category = "Character Selection")
	void RefreshSlots();

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SlotButton0 = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SlotButton1 = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SlotButton2 = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SlotState0 = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SlotState1 = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SlotState2 = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText = nullptr;

	// 이동할 레벨은 WBP_CharacterSelection의 클래스 기본값에서 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Selection|Travel")
	TSoftObjectPtr<UWorld> CustomizationLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Selection|Travel")
	TSoftObjectPtr<UWorld> GameplayLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Selection|Travel")
	TSubclassOf<AGameModeBase> GameplayGameModeClass;

private:
	UFUNCTION() void HandleSlot0Clicked();
	UFUNCTION() void HandleSlot1Clicked();
	UFUNCTION() void HandleSlot2Clicked();
	UFUNCTION() void HandleStartClicked();

	TArray<UButton*> GetSlotButtons() const;
	TArray<UTextBlock*> GetSlotStateTexts() const;
};
