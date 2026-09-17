#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "../Frontend/UEFrontendPanelStyle.h"
#include "UELoginWidget.generated.h"

class UButton;
class UEditableTextBox;
class UImage;
class USizeBox;
class UTextBlock;
class UWidgetAnimation;
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

	/** 로그인·회원가입 전환 때 부른다. bUseTabStyles 를 끄면 선택된 탭 모양은 WBP 가 여기서 고른다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Login")
	void OnScreenModeChanged(EUELoginScreenMode Mode);

	// 상태 종류별 문구 색과 아이콘. 비어 있으면 C++ 은 모양을 건드리지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Style")
	TMap<EUEFrontendStatusKind, FUEFrontendStatusStyle> StatusStyles;

	// 켜면 오류 원인이 된 입력칸만 InputErrorStyle 로, 나머지는 InputStyle 로 둔다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Style")
	bool bUseInputErrorStyle = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Style", meta = (EditCondition = "bUseInputErrorStyle"))
	FEditableTextBoxStyle InputStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Style", meta = (EditCondition = "bUseInputErrorStyle"))
	FEditableTextBoxStyle InputErrorStyle;

	// 켜면 로그인·회원가입 탭 중 선택된 쪽에 SelectedTabStyle 을 입힌다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Style")
	bool bUseTabStyles = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Style", meta = (EditCondition = "bUseTabStyles"))
	FButtonStyle TabStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Style", meta = (EditCondition = "bUseTabStyles"))
	FButtonStyle SelectedTabStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Style", meta = (EditCondition = "bUseTabStyles"))
	FLinearColor TabTextColor = FLinearColor::Black;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Style", meta = (EditCondition = "bUseTabStyles"))
	FLinearColor SelectedTabTextColor = FLinearColor::White;

	UFUNCTION(BlueprintCallable, Category = "Login")
	void SetStatusMessage(const FText& Message, EUEFrontendStatusKind Kind = EUEFrontendStatusKind::Info);

	/** 상태 문구가 바뀔 때마다 부른다. 색·아이콘·연결 중 파형은 WBP 가 Kind 를 보고 고른다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Login")
	void OnStatusChanged(EUEFrontendStatusKind Kind);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshScreenMode();
	void RefreshTabs();
	void ShowUserIdFormatGuide(const FString& UserId);

	// ErrorInput 은 Kind 가 Error 일 때 오류 테두리로 표시할 입력칸이다.
	void SetStatus(const FText& Message, EUEFrontendStatusKind Kind, UEditableTextBox* ErrorInput);
	void MarkInputError(UEditableTextBox* Input, bool bError);
	void HandleLogin();
	void HandleRegistration();
	UUEAccountSubsystem* GetAccountSubsystem() const;
	FText GetRegistrationResultMessage(EUELocalAccountResult Result) const;

	UFUNCTION()
	void HandleRegisterTabClicked();

	UFUNCTION()
	void HandleLoginTabClicked();

	UFUNCTION()
	void HandlePrimaryActionClicked();

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
	// 탭 배치에서는 탭 글자가 제목을 대신하므로 없어도 된다.
	UPROPERTY(meta = (BindWidgetOptional))
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
	TObjectPtr<UEditableTextBox> PasswordInputBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> ConfirmPasswordInputBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> ConfirmPasswordRow = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RegisterTabButton = nullptr;

	/**
	 * 로그인·회원가입을 탭으로 나란히 둘 때의 로그인 탭. 없으면 예전 배치로 본다:
	 * 회원가입 중에는 RegisterTabButton 을 숨기고 BackButton 이 로그인으로 돌아간다.
	 * 있으면 두 탭이 항상 보이고 BackButton 은 모드와 상관없이 서버 화면으로 간다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LoginTabButton = nullptr;

	// bUseTabStyles 가 켜져 있을 때 탭 글자색과 선택 눈금을 바꾼다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LoginTabLabel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RegisterTabLabel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LoginTabTick = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RegisterTabTick = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PrimaryActionButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PrimaryActionLabel = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusBlock = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> StatusIcon = nullptr;

	// 연결 중일 때 아이콘 대신 보이는 파형. 움직임은 StatusWaveLoop 애니메이션이 맡는다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StatusWave = nullptr;

	// 화면이 뜰 때 패널이 들어오는 애니메이션.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> PanelIn = nullptr;

	// 파형처럼 화면이 떠 있는 동안 계속 도는 움직임.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> StatusWaveLoop = nullptr;

	// 로그인 패널 머리줄에 지금 붙을 서버 주소를 보여 준다. 누르면 서버 화면으로 가는
	// BackButton 옆에 두는 용도라 없어도 된다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ServerAddressText = nullptr;

	EUELoginScreenMode ScreenMode = EUELoginScreenMode::Login;

	// 지금 오류 스타일이 입혀진 입력칸. 스타일을 매번 다시 넣지 않으려고 들고 있다.
	UPROPERTY(Transient)
	TSet<TObjectPtr<UEditableTextBox>> InputsShowingError;

	// 키보드·패드로 버튼에 포커스가 가면 “<버튼 이름>FocusRing” 위젯을 보여 준다.
	FUEFrontendFocusRings FocusRings;

	// 서버 응답을 기다리는 중이다. 연타로 두 번째 요청이 나가면 서버가 연결을
	// 끊으므로(LoginHandler 의 Busy 단계) 버튼을 잠근다.
	bool bRequestPending = false;

	// 방금 보낸 로그인 요청의 아이디. 성공 응답에 아이디가 실려 오지 않아서
	// 여기 들고 있다가 OnLoginSucceeded 에 싣는다.
	FString PendingUserId;
};
