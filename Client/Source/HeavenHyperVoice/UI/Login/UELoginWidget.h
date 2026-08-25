#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UELoginWidget.generated.h"

class UButton;
class UEditableTextBox;
class USizeBox;
class UTextBlock;
class UUEAccountSubsystem;
enum class EUELocalAccountResult : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUELoginSucceededSignature, const FString&, UserId, const FString&, Nickname);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUEAccountRegisteredSignature, const FString&, UserId, const FString&, Nickname);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUELoginBackRequestedSignature);

UENUM(BlueprintType)
enum class EUELoginScreenMode : uint8
{
	Login,
	Register
};

/** 로그인·회원가입 WBP의 입력과 로컬 계정 검증만 연결하는 네이티브 기반 위젯이다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUELoginWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UUELoginWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText LoginSubtitleText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Text")
	FText RegisterSubtitleText;

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

	// 검증 문구도 WBP 기본값에서 바꿀 수 있게 코드 문자열을 두지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText StorageUnavailableStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText NicknameRequiredStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText InvalidNicknameLengthStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText UserIdRequiredStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText InvalidUserIdLengthStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText InvalidUserIdCharactersStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText InvalidPasswordLengthStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText PasswordMismatchStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText AccountAlreadyExistsStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText SaveFailedStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText InvalidFieldsStatusText;

	UPROPERTY(BlueprintAssignable, Category = "Login|Event")
	FUELoginSucceededSignature OnLoginSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "Login|Event")
	FUEAccountRegisteredSignature OnAccountRegistered;

	UPROPERTY(BlueprintAssignable, Category = "Login|Event")
	FUELoginBackRequestedSignature OnBackRequested;

	UFUNCTION(BlueprintCallable, Category = "Login")
	void SetScreenMode(EUELoginScreenMode NewMode);

	UFUNCTION(BlueprintPure, Category = "Login")
	EUELoginScreenMode GetScreenMode() const { return ScreenMode; }

	UFUNCTION(BlueprintCallable, Category = "Login")
	void SetStatusMessage(const FText& Message);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void RefreshScreenMode();
	void ResetDuplicateCheck();
	void HandleLogin();
	void HandleRegistration();
	UUEAccountSubsystem* GetAccountSubsystem() const;
	FText GetRegistrationResultMessage(EUELocalAccountResult Result) const;

	UFUNCTION()
	void HandleRegisterTabClicked();

	UFUNCTION()
	void HandlePrimaryActionClicked();

	UFUNCTION()
	void HandleDuplicateCheckClicked();

	UFUNCTION()
	void HandleUserIdChanged(const FText& NewText);

	UFUNCTION()
	void HandleBackClicked();

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SubtitleBlock = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> NicknameInputBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> NicknameRow = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> IdInputBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DuplicateCheckButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> PasswordInputBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> ConfirmPasswordInputBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> ConfirmPasswordRow = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RegisterTabButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PrimaryActionButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PrimaryActionLabel = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusBlock = nullptr;

	EUELoginScreenMode ScreenMode = EUELoginScreenMode::Login;
	bool bUserIdDuplicateChecked = false;
	FString DuplicateCheckedUserId;
};
