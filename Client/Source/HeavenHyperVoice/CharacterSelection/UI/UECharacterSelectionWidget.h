#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UECharacterLobbySlotWidget.h"
#include "UECharacterSelectionWidget.generated.h"

class AGameModeBase;
class UTextBlock;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUECharacterCreationRequestedSignature, int32, SlotIndex);

/** 로컬 캐릭터 슬롯을 로비 UMG에 연결한다. 서버 통신은 이 위젯의 책임이 아니다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUECharacterSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Lobby|Event")
	FUECharacterCreationRequestedSignature OnCharacterCreationRequested;

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RefreshLobby();

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SetAccountName(const FText& InAccountName);

	// 기존 블루프린트 호출이 깨지지 않도록 남겨 둔 호환 함수다.
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SelectSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void StartSelectedCharacter();

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RefreshSlots();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUECharacterLobbySlotWidget> LobbySlot0 = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUECharacterLobbySlotWidget> LobbySlot1 = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUECharacterLobbySlotWidget> LobbySlot2 = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AccountNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OccupiedCountValueText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TotalCountValueText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText = nullptr;

	// 로그인 연동 전에도 로비를 독립적으로 확인할 수 있는 표시 이름이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Display")
	FText DefaultAccountName;

	// 이름과 파트너의 디자이너 미리보기 값은 WBP_CharacterSelection 기본값에서 바꾼다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Display")
	TArray<FUECharacterLobbySlotViewData> SlotDisplayDefaults;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Text")
	FText NoCharacterSelectedMessage;

	// 캐릭터를 고르고 서버 응답을 기다리는 동안 보여준다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Text")
	FText EnteringMessage;

	// 입장할 레벨과 게임 모드는 블루프린트 기본값에서 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Travel")
	TSoftObjectPtr<UWorld> GameplayLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Travel")
	TSubclassOf<AGameModeBase> GameplayGameModeClass;

private:
	UFUNCTION()
	void HandleLobbySlotAction(int32 SlotIndex);

	UFUNCTION()
	void HandleLobbySlotDelete(int32 SlotIndex);

	// 입장은 서버 왕복이다. 캐릭터를 고르면 서버가 필드·채팅 티켓을 발급하고,
	// 그게 도착해야 게임 레벨로 넘어간다. 티켓 없이 넘어가면 필드 서버가
	// 입장을 거절한다.
	UFUNCTION()
	void HandleServerEnterReady(const FString& Nickname);

	UFUNCTION()
	void HandleServerActionFailed(bool bOk, const FString& Message);

	void EnterSelectedCharacter();
	TArray<UUECharacterLobbySlotWidget*> GetLobbySlots() const;
};
