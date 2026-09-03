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

	// 서버 왕복 중에 보여준다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText ConnectingStatusText;

	// 아이디 형식만 통과했다는 뜻이다. 중복 여부는 가입할 때 서버가 답한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Text")
	FText UserIdFormatOkStatusText;

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

	// --- 로그인 서버 응답 ---
	//
	// 로그인과 가입은 서버 왕복이라 즉시 결과가 나오지 않는다. 요청을 보낸 뒤
	// 버튼을 잠그고, 아래 콜백에서 푼다.

	UFUNCTION()
	void HandleServerLoginCompleted(bool bOk, const FString& Message);

	UFUNCTION()
	void HandleServerRegisterCompleted(bool bOk, const FString& Message);

	UFUNCTION()
	void HandleServerDisconnected(bool bOk, const FString& Message);

	class UUEGameInstance* GetHHVGameInstance() const;
	void SetRequestPending(bool bPending);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SubtitleBlock = nullptr;

	/**
	 * 가입 화면의 닉네임 칸. 항상 접어 둔다.
	 *
	 * 계정과 캐릭터가 갈리면서 닉네임은 캐릭터 속성이 됐다. 가입은 아이디와
	 * 비밀번호만 보내고 (RegisterRequest 에 닉네임 필드가 없다), 이름은 캐릭터를
	 * 만들 때 받는다.
	 *
	 * BindWidgetOptional 이라 WBP 에서 이 칸을 통째로 지워도 된다. 지우기
	 * 전까지는 여기서 접는다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
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

	// 서버 응답을 기다리는 중이다. 연타로 두 번째 요청이 나가면 서버가 연결을
	// 끊으므로(LoginHandler 의 Busy 단계) 버튼을 잠근다.
	bool bRequestPending = false;

	// 방금 보낸 로그인 요청의 아이디. 성공 응답에 아이디가 실려 오지 않아서
	// 여기 들고 있다가 OnLoginSucceeded 에 싣는다.
	FString PendingUserId;
};
