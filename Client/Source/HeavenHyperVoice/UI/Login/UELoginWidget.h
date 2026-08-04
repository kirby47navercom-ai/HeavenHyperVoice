// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UELoginWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UHorizontalBox;
class USizeBox;
class UTextBlock;
class UUEAccountSubsystem;
class UVerticalBox;
enum class EUELocalAccountResult : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUELoginSucceededSignature, const FString&, UserId, const FString&, Nickname);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUEAccountRegisteredSignature, const FString&, UserId, const FString&, Nickname);

UENUM(BlueprintType)
enum class EUELoginScreenMode : uint8
{
	Login,
	Register
};

/**
 * Client login and registration screen.
 *
 * The widget owns presentation and duplicate-check state only. Account storage,
 * password hashing, and credential decisions belong to UUEAccountSubsystem.
 */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUELoginWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UUELoginWidget(const FObjectInitializer& ObjectInitializer);

	// Every visible label can be changed from a Blueprint child class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText TitleText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText LoginSubtitleText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText RegisterSubtitleText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText LoginTabText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText RegisterTabText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText NicknameHintText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText IdHintText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText PasswordHintText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText ConfirmPasswordHintText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText CheckDuplicateButtonText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText LoginButtonText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText CreateAccountButtonText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText ReadyStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText RegisterReadyStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText DuplicateCheckRequiredStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText UserIdAvailableStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText LoginFailedStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText LoginSucceededStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText RegistrationSucceededStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Style")
	FLinearColor InputTextColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout", meta = (ClampMin = "320.0"))
	float PanelWidth = 460.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout", meta = (ClampMin = "32.0"))
	float InputHeight = 46.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Layout", meta = (ClampMin = "32.0"))
	float ButtonHeight = 48.0f;

	UPROPERTY(BlueprintAssignable, Category = "Login|Event")
	FUELoginSucceededSignature OnLoginSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "Login|Event")
	FUEAccountRegisteredSignature OnAccountRegistered;

	UFUNCTION(BlueprintCallable, Category = "Login")
	void SetScreenMode(EUELoginScreenMode NewMode);

	UFUNCTION(BlueprintPure, Category = "Login")
	EUELoginScreenMode GetScreenMode() const { return ScreenMode; }

	UFUNCTION(BlueprintCallable, Category = "Login")
	void SetStatusMessage(const FText& Message);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildLoginLayout();
	void RefreshScreenMode();
	void ResetDuplicateCheck();
	void HandleLogin();
	void HandleRegistration();
	UUEAccountSubsystem* GetAccountSubsystem() const;
	FText GetRegistrationResultMessage(EUELocalAccountResult Result) const;

	UFUNCTION()
	void HandleLoginTabClicked();

	UFUNCTION()
	void HandleRegisterTabClicked();

	UFUNCTION()
	void HandlePrimaryActionClicked();

	UFUNCTION()
	void HandleDuplicateCheckClicked();

	UFUNCTION()
	void HandleUserIdChanged(const FText& NewText);

private:
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BackgroundBorder = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleBlock = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> NicknameInputBox = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> NicknameRow = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> IdInputBox = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DuplicateCheckButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> PasswordInputBox = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> ConfirmPasswordInputBox = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ConfirmPasswordRow = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LoginTabButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RegisterTabButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PrimaryActionButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PrimaryActionLabel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusBlock = nullptr;

	EUELoginScreenMode ScreenMode = EUELoginScreenMode::Login;
	bool bUserIdDuplicateChecked = false;
	FString DuplicateCheckedUserId;
};
