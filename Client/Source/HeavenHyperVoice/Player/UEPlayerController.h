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
class UScrollBox;
class UUserWidget;
class UVerticalBox;
class UUEDataAsset;
class UUEPokemonPartyWidget;

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
	void CloseChatInput();

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

private:
	void CreateChatWidget();
	void StartChat();
	bool SubmitChatText(const FString& Text);
	void AddChatLine(const FString& Nickname, const FString& Text, bool bSystem);
	void AddSystemMessage(const FString& Text);

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

	std::unique_ptr<FHHVChatConnection> ChatConnection;
	bool bChatInputOpen = false;
	int32 ChatMessageCount = 0;
	static constexpr int32 MaxVisibleChatMessages = 80;
};
