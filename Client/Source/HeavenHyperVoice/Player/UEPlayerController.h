// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/EditableTextBox.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "../Net/HHVChatConnection.h"
#include <memory>
#include "UEPlayerController.generated.h"

class AUEPlayerCharacter;
class UBorder;
class UScrollBox;
class UTextBlock;
class UUserWidget;
class UVerticalBox;
class UWidget;
class UUEDataAsset;
class UUEPokemonPartyWidget;
class UUEOptionsMenuWidget;
class UUEOptionsHUDWidget;
class UUEOptionsScreenWidget;
class UUEGameSettingsWidget;
class UUEOptionsConfirmWidget;

/** 실제 플레이 레벨의 이동과 액션 입력을 처리한다. */
UCLASS()
class HEAVENHYPERVOICE_API AUEPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AUEPlayerController();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OpenChatInput();
	void CloseChatInput(bool bClearDraft = true);

	/** 가장 최근에 선택한 창을 맨 앞으로 올린다. WBP 자식은 UEWindowWidget을 사용한다. */
	UFUNCTION(BlueprintCallable, Category = "UI|Windows")
	void ActivateUIWindow(UUserWidget* Window, bool bFocus = true);
	void DeactivateUIWindow(UUserWidget* Window);
	bool HandleChatWindowKey(UUserWidget* Window, const FKeyEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "Options")
	void ToggleOptionsMenu();

	// WBP 클래스가 지정된 경우에만 HUD를 만든다. 에셋 경로는 코드에서 찾지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|UI")
	void ShowPokemonPartyWidget();

	UFUNCTION(BlueprintPure, Category = "Pokemon|UI")
	UUEPokemonPartyWidget* GetPokemonPartyWidget() const { return PokemonPartyWidget; }

	// --- 콘솔 명령 (~ 로 열어서 입력) ---
	//
	// 인스턴스 입구 액터가 나오기 전까지 들어가고 나오는 유일한 길이다.
	// 입력 액션과 태그를 만들면 그쪽에서 같은 함수를 부르면 된다.

	/** 인스턴스로 들어간다. 레벨과 접속 서버가 함께 바뀐다. */
	UFUNCTION(Exec)
	void HHVEnterInstance(int32 InstanceType = 1);

	/** 인스턴스에서 나와 필드로 돌아간다. */
	UFUNCTION(Exec)
	void HHVLeaveInstance();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Options")
	TSubclassOf<UUEOptionsMenuWidget> OptionsMenuClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Options")
	TSubclassOf<UUEOptionsHUDWidget> OptionsHUDClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Options")
	TSubclassOf<UUEGameSettingsWidget> GameSettingsClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Options")
	TSubclassOf<UUEOptionsConfirmWidget> OptionsConfirmClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Options")
	TSoftObjectPtr<UWorld> FrontendLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UUEDataAsset> InputData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Look", meta = (ClampMin = "0.0"))
	float LookYawRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Look", meta = (ClampMin = "0.0"))
	float LookPitchRate = 1.0f;

	// 실제 화면 디자인은 이 변수에 지정한 WBP_PokemonParty가 담당한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|UI")
	TSubclassOf<UUEPokemonPartyWidget> PokemonPartyWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|UI")
	int32 PokemonPartyWidgetZOrder = 10;

	/** 실제 채팅 배치와 스타일은 이 UMG 에셋들에서 수정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chat|UI")
	TSubclassOf<UUserWidget> ChatWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chat|UI")
	TSubclassOf<UUserWidget> ChatLineWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chat|UI")
	TSubclassOf<UUserWidget> ChatSystemLineWidgetClass;

public:
	// --- 플레이어 파티 ---
	//
	// 명령은 전부 채팅 연결로 나간다. 이 연결이 필드에서도 인스턴스에서도
	// 살아 있는 유일한 통로다. 명단은 UUEGameInstance 가 들고 있다.

	UFUNCTION(BlueprintCallable, Category = "HHV|Party")
	void RequestPartyInvite(const FString& TargetNickname);

	UFUNCTION(BlueprintCallable, Category = "HHV|Party")
	void RequestPartyAccept(int64 PartyId);

	UFUNCTION(BlueprintCallable, Category = "HHV|Party")
	void RequestPartyDecline(int64 PartyId);

	UFUNCTION(BlueprintCallable, Category = "HHV|Party")
	void RequestPartyLeave();

	UFUNCTION(BlueprintCallable, Category = "HHV|Party")
	void RequestPartyKick(int64 TargetAccountId);

	/** 파티장만 부를 수 있다. 서버가 전원에게 입장을 열어 준다. */
	UFUNCTION(BlueprintCallable, Category = "HHV|Party")
	void RequestPartyEnterInstance(int32 InstanceType);

	/** 화면에 시스템 문구 한 줄. 포탈이 파티 안내를 띄우는 데도 쓴다. */
	void AddSystemMessage(const FString& Text);

private:
	void HandleEscape();
	void HandleGameViewportClick();
	void CloseOptionsMenu();
	UFUNCTION()
	void HandleOptionsAction(FName ActionId);
	UFUNCTION()
	void HandleOptionsScreenAction(FName ActionId);
	void ShowOptionsScreen(UUEOptionsScreenWidget* Screen);
	void RemoveOptionsScreen();
	UPROPERTY(Transient)
	TObjectPtr<UUEOptionsScreenWidget> OptionsScreen;
	UPROPERTY(Transient)
	TObjectPtr<UUEOptionsMenuWidget> OptionsMenu;
	UPROPERTY(Transient)
	TObjectPtr<UUEOptionsHUDWidget> OptionsHUD;
	bool bReturningToFrontend = false;

	// 마지막으로 받은 초대. /수락 이 이걸 쓴다. 서버 쪽 초대장은 60초에 만료된다.
	int64 PendingInvitePartyId = 0;

	void CreateChatWidget();
	void StartChat();
	bool SubmitChatText(const FString& Text);
	void AddChatLine(const FString& Nickname, const FString& Text, bool bSystem,
		EUEChatChannel Channel);

	// 지금 고른 탭에 맞춰 줄을 보이고 숨긴다. 탭을 바꿀 때마다 부른다.
	void RefreshChatFilter();

	// 탭 번호를 발화 채널로 바꾼다. "전체" 탭에서 치면 일반으로 나간다 —
	// 모든 채널에 동시에 말하는 것은 없다.
	EUEChatChannel ChannelForSelectedTab() const;
	void SuspendChatInput();
	void RefreshWindowOrder();
	void RefreshWindowInput(bool bFocus = true);
	TArray<TWeakObjectPtr<UUserWidget>> UIWindowOrder;
	bool bWindowMoveBlocked = false;
	bool bWindowLookBlocked = false;

	// T로 채팅창 전체를 켜고 끈다. 시야를 가리는 채팅창을 빠르게 숨길 때 쓴다.
	void ToggleChatVisible();

	UFUNCTION()
	void HandleChatTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void AddDefaultMappingContext() const;
	void BindGameplayInput();
	void BindMoveInput(class UEnhancedInputComponent* EnhancedInputComponent);
	void BindLookInput(class UEnhancedInputComponent* EnhancedInputComponent);
	void BindActionInput(class UEnhancedInputComponent* EnhancedInputComponent);
	void BindRunInput(class UEnhancedInputComponent* EnhancedInputComponent);
	void BindJumpInput(UEnhancedInputComponent* EnhancedInputComponent);
	void BindRollInput(UEnhancedInputComponent* EnhancedInputComponent);
	void BindPokemonAttackInput(UEnhancedInputComponent* EnhancedInputComponent);
	void BindMouseViewInput(UEnhancedInputComponent* EnhancedInputComponent);
	void BindChatInput(UEnhancedInputComponent* EnhancedInputComponent);
	AUEPlayerCharacter* GetControlledPlayerCharacter() const;
	bool HasPendingHHVAppearance() const;
	void PushMovementInputToCharacter();

	void HandleMove(const FInputActionValue& Value);
	void HandleMoveStopped(const FInputActionValue& Value);
	void HandleMoveForward(const FInputActionValue& Value);
	void HandleMoveForwardStopped(const FInputActionValue& Value);
	void HandleMoveBackward(const FInputActionValue& Value);
	void HandleMoveBackwardStopped(const FInputActionValue& Value);
	void HandleMoveRight(const FInputActionValue& Value);
	void HandleMoveRightStopped(const FInputActionValue& Value);
	void HandleMoveLeft(const FInputActionValue& Value);
	void HandleMoveLeftStopped(const FInputActionValue& Value);
	void HandleLookYaw(const FInputActionValue& Value);
	void HandleLookPitch(const FInputActionValue& Value);
	void HandleRunStarted(const FInputActionValue& Value);
	void HandleRunStopped(const FInputActionValue& Value);
	void HandleJump(const FInputActionValue& Value);
	void HandleRoll(const FInputActionValue& Value);
	void HandlePokemonToggle(const FInputActionValue& Value);
	void HandlePokemonAttack1(const FInputActionValue& Value);
	void HandlePokemonAttack2(const FInputActionValue& Value);
	void HandlePokemonAttack3(const FInputActionValue& Value);
	void HandlePokemonAttack4(const FInputActionValue& Value);
	void HandlePokemonAttackSlot(int32 AttackSlot);
	void HandleMouseViewStarted(const FInputActionValue& Value);
	void HandleMouseViewStopped(const FInputActionValue& Value);
	void HandleChatInputAction(const FInputActionValue& Value);
	void SelectChatChannel(int32 ChannelIndex);

	UFUNCTION()
	FEventReply HandleChannelTab0MouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	UFUNCTION()
	FEventReply HandleChannelTab1MouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	UFUNCTION()
	FEventReply HandleChannelTab2MouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	UFUNCTION()
	FEventReply HandleChannelTab3MouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	UFUNCTION()
	FEventReply HandleChatInputMouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	UFUNCTION()
	FEventReply HandleChatHeaderMouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	UFUNCTION()
	FEventReply HandleChatHeaderMouseButtonUp(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	UFUNCTION()
	FEventReply HandleChatHeaderMouseMove(FGeometry MyGeometry, const FPointerEvent& MouseEvent);


	FVector2D PendingMovementInput = FVector2D::ZeroVector;
	float MaxWalkSpeed = 260.0f;
	float RunCross = 1.5f;

	UPROPERTY(Transient)
	TObjectPtr<UUEPokemonPartyWidget> PokemonPartyWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ChatWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> ChatInput = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ChatMessageScroll = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ChatMessageList = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ChatMovablePanel = nullptr;



	UPROPERTY(Transient)
	TObjectPtr<UBorder> ChatDragHandle = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ChatInputBackground = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ChatChannelTabs;

	// 0 = 전체(필터), 1 = 일반, 2 = 파티, 3 = 전투.
	// "전체" 는 채널이 아니라 보기 방식이라 발화에는 쓰이지 않는다.
	int32 SelectedChatTab = 0;

	// 줄마다 어느 채널인지. ChatMessageList 의 자식 순서와 1:1 로 맞춘다 —
	// 둘 다 앞에서부터 같이 잘라내므로 색인이 어긋나지 않는다.
	TArray<EUEChatChannel> ChatLineChannels;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ChatInputChannelText = nullptr;

	std::unique_ptr<FHHVChatConnection> ChatConnection;
	bool bChatInputOpen = false;
	bool bMouseViewHeld = false;
	bool bDraggingChat = false;
	// T로 채팅창 전체를 감춘 상태다.
	bool bChatHidden = false;

	// 감추기 전 가시성을 저장해 WBP가 정한 입력 처리 상태까지 그대로 복원한다.
	ESlateVisibility ChatVisibilityBeforeHide = ESlateVisibility::SelfHitTestInvisible;
	FVector2D LastChatDragMousePosition = FVector2D::ZeroVector;
	FLinearColor SelectedChatTabColor = FLinearColor::White;
	FLinearColor NormalChatTabColor = FLinearColor::White;
	int32 ChatMessageCount = 0;
	static constexpr int32 MaxVisibleChatMessages = 80;
};
