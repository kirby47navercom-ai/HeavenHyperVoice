#include "UELoginWidget.h"

// 로컬 계정 시스템은 더 이상 로그인 경로에서 쓰지 않는다. 인증은 LoginServer 가
// 한다. 헤더는 GetRegistrationResultMessage 가 아직 참조해서 남겨둔다.
#include "../../System/Account/UEAccountSubsystem.h"
#include "../../System/UEGameInstance.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

UUELoginWidget::UUELoginWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UUELoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RegisterTabButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRegisterTabClicked);
	PrimaryActionButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePrimaryActionClicked);
	DuplicateCheckButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDuplicateCheckClicked);
	IdInputBox->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleUserIdChanged);
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);

	if (UUEGameInstance* GameInstance = GetHHVGameInstance())
	{
		GameInstance->OnLoginCompleted.AddUniqueDynamic(this, &ThisClass::HandleServerLoginCompleted);
		GameInstance->OnRegisterCompleted.AddUniqueDynamic(this, &ThisClass::HandleServerRegisterCompleted);
		GameInstance->OnServerDisconnected.AddUniqueDynamic(this, &ThisClass::HandleServerDisconnected);
	}

	SetScreenMode(EUELoginScreenMode::Login);
	SetRequestPending(false);
}

void UUELoginWidget::NativeDestruct()
{
	if (RegisterTabButton)
	{
		RegisterTabButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRegisterTabClicked);
	}
	if (PrimaryActionButton)
	{
		PrimaryActionButton->OnClicked.RemoveDynamic(this, &ThisClass::HandlePrimaryActionClicked);
	}
	if (DuplicateCheckButton)
	{
		DuplicateCheckButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleDuplicateCheckClicked);
	}
	if (IdInputBox)
	{
		IdInputBox->OnTextChanged.RemoveDynamic(this, &ThisClass::HandleUserIdChanged);
	}
	if (BackButton)
	{
		BackButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleBackClicked);
	}

	// GameInstance 는 이 위젯보다 오래 산다. 떼지 않으면 파괴된 위젯으로 콜백이 간다.
	if (UUEGameInstance* GameInstance = GetHHVGameInstance())
	{
		GameInstance->OnLoginCompleted.RemoveDynamic(this, &ThisClass::HandleServerLoginCompleted);
		GameInstance->OnRegisterCompleted.RemoveDynamic(this, &ThisClass::HandleServerRegisterCompleted);
		GameInstance->OnServerDisconnected.RemoveDynamic(this, &ThisClass::HandleServerDisconnected);
	}

	Super::NativeDestruct();
}

void UUELoginWidget::SetScreenMode(EUELoginScreenMode NewMode)
{
	ScreenMode = NewMode;
	ResetDuplicateCheck();
	RefreshScreenMode();
	SetStatusMessage(ScreenMode == EUELoginScreenMode::Register ? RegisterReadyStatusText : ReadyStatusText);
}

void UUELoginWidget::SetStatusMessage(const FText& Message)
{
	if (StatusBlock)
	{
		StatusBlock->SetText(Message);
	}
}

void UUELoginWidget::RefreshScreenMode()
{
	const bool bIsRegisterMode = ScreenMode == EUELoginScreenMode::Register;
	SubtitleBlock->SetText(bIsRegisterMode ? RegisterSubtitleText : LoginSubtitleText);
	NicknameRow->SetVisibility(bIsRegisterMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	DuplicateCheckButton->SetVisibility(bIsRegisterMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ConfirmPasswordRow->SetVisibility(bIsRegisterMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RegisterTabButton->SetVisibility(bIsRegisterMode ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	PrimaryActionLabel->SetText(bIsRegisterMode ? CreateAccountButtonText : LoginButtonText);
}

void UUELoginWidget::ResetDuplicateCheck()
{
	bUserIdDuplicateChecked = false;
	DuplicateCheckedUserId.Reset();
}

void UUELoginWidget::HandleRegisterTabClicked()
{
	SetScreenMode(EUELoginScreenMode::Register);
}

void UUELoginWidget::HandlePrimaryActionClicked()
{
	if (ScreenMode == EUELoginScreenMode::Login)
	{
		HandleLogin();
		return;
	}

	HandleRegistration();
}

void UUELoginWidget::HandleDuplicateCheckClicked()
{
	// 서버에는 "이 아이디 쓸 수 있나" 를 묻는 메시지가 없다. 넣으려면 프로토콜과
	// 서버 핸들러가 늘어나는데, 얻는 것은 가입 버튼을 누르기 전에 알려주는 것뿐이다.
	// 중복이면 가입 응답이 "이미 사용 중인 아이디입니다" 로 알려준다.
	//
	// 그래서 여기서는 서버가 어차피 거절할 형식만 미리 걸러준다. 규칙은
	// LoginCodec.h 의 isValidUsername 과 같다: 영문/숫자/밑줄, 3~32자.
	const FString UserId = IdInputBox->GetText().ToString().TrimStartAndEnd();

	if (UserId.Len() < 3 || UserId.Len() > 32)
	{
		ResetDuplicateCheck();
		SetStatusMessage(InvalidUserIdLengthStatusText);
		return;
	}

	for (const TCHAR Character : UserId)
	{
		const bool bAllowed = (Character >= TEXT('a') && Character <= TEXT('z'))
			|| (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('0') && Character <= TEXT('9'))
			|| Character == TEXT('_');
		if (!bAllowed)
		{
			ResetDuplicateCheck();
			SetStatusMessage(InvalidUserIdCharactersStatusText);
			return;
		}
	}

	bUserIdDuplicateChecked = true;
	DuplicateCheckedUserId = UserId.ToLower();
	SetStatusMessage(UserIdFormatOkStatusText);
}

void UUELoginWidget::HandleBackClicked()
{
	if (ScreenMode == EUELoginScreenMode::Register)
	{
		SetScreenMode(EUELoginScreenMode::Login);
		return;
	}

	OnBackRequested.Broadcast();
}

void UUELoginWidget::HandleUserIdChanged(const FText& NewText)
{
	// 중복 확인 뒤 아이디를 수정하면 이전 확인 결과를 바로 무효화한다.
	if (bUserIdDuplicateChecked && NewText.ToString().TrimStartAndEnd().ToLower() != DuplicateCheckedUserId)
	{
		ResetDuplicateCheck();
		if (ScreenMode == EUELoginScreenMode::Register)
		{
			SetStatusMessage(DuplicateCheckRequiredStatusText);
		}
	}
}

UUEGameInstance* UUELoginWidget::GetHHVGameInstance() const
{
	return GetWorld() ? Cast<UUEGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
}

void UUELoginWidget::SetRequestPending(bool bPending)
{
	bRequestPending = bPending;
	if (PrimaryActionButton)
	{
		PrimaryActionButton->SetIsEnabled(!bPending);
	}
	if (DuplicateCheckButton)
	{
		DuplicateCheckButton->SetIsEnabled(!bPending);
	}
}

void UUELoginWidget::HandleLogin()
{
	UUEGameInstance* GameInstance = GetHHVGameInstance();
	if (!GameInstance)
	{
		SetStatusMessage(LoginFailedStatusText);
		return;
	}

	const FString UserId = IdInputBox->GetText().ToString().TrimStartAndEnd();
	const FString Password = PasswordInputBox->GetText().ToString();
	if (UserId.IsEmpty() || Password.IsEmpty())
	{
		SetStatusMessage(InvalidFieldsStatusText);
		return;
	}

	// 서버 왕복이라 여기서 결과가 나오지 않는다. HandleServerLoginCompleted 에서 잇는다.
	PendingUserId = UserId;
	SetRequestPending(true);
	SetStatusMessage(ConnectingStatusText);
	GameInstance->ConnectAndLogin(UserId, Password);
}

void UUELoginWidget::HandleServerLoginCompleted(bool bOk, const FString& Message)
{
	SetRequestPending(false);

	if (!bOk)
	{
		// 서버가 준 사유를 그대로 보여준다. 아이디 존재 여부는 서버가 이미
		// 하나의 문구로 합쳐서 보낸다.
		SetStatusMessage(Message.IsEmpty() ? LoginFailedStatusText : FText::FromString(Message));
		return;
	}

	SetStatusMessage(LoginSucceededStatusText);
	PasswordInputBox->SetText(FText::GetEmpty());

	// 닉네임은 캐릭터 속성이라 로그인 응답에 없다. 캐릭터를 고를 때 정해진다.
	OnLoginSucceeded.Broadcast(PendingUserId, FString());
}

void UUELoginWidget::HandleServerRegisterCompleted(bool bOk, const FString& Message)
{
	SetRequestPending(false);

	if (!bOk)
	{
		ResetDuplicateCheck();
		SetStatusMessage(Message.IsEmpty() ? SaveFailedStatusText : FText::FromString(Message));
		return;
	}

	OnAccountRegistered.Broadcast(PendingUserId, FString());
	PasswordInputBox->SetText(FText::GetEmpty());
	ConfirmPasswordInputBox->SetText(FText::GetEmpty());
	NicknameInputBox->SetText(FText::GetEmpty());
	ScreenMode = EUELoginScreenMode::Login;
	ResetDuplicateCheck();
	RefreshScreenMode();
	SetStatusMessage(Message.IsEmpty() ? RegistrationSucceededStatusText : FText::FromString(Message));
}

void UUELoginWidget::HandleServerDisconnected(bool bOk, const FString& Message)
{
	// 요청 도중 끊긴 경우다. 버튼을 풀어 다시 시도할 수 있게 한다.
	SetRequestPending(false);
	SetStatusMessage(Message.IsEmpty() ? LoginFailedStatusText : FText::FromString(Message));
}

void UUELoginWidget::HandleRegistration()
{
	UUEGameInstance* GameInstance = GetHHVGameInstance();
	if (!GameInstance)
	{
		SetStatusMessage(StorageUnavailableStatusText);
		return;
	}

	const FString UserId = IdInputBox->GetText().ToString().TrimStartAndEnd();
	const FString Password = PasswordInputBox->GetText().ToString();
	const FString Confirmation = ConfirmPasswordInputBox->GetText().ToString();

	// 비밀번호 확인은 서버에 보낼 값이 아니라 입력 실수를 잡는 것이라 여기서 본다.
	if (Password != Confirmation)
	{
		SetStatusMessage(PasswordMismatchStatusText);
		return;
	}
	if (UserId.IsEmpty() || Password.IsEmpty())
	{
		SetStatusMessage(InvalidFieldsStatusText);
		return;
	}

	// 닉네임은 계정이 아니라 캐릭터 속성이라 가입에 싣지 않는다. 캐릭터를 만들 때
	// 받는다. 아이디 중복은 서버가 가입 응답으로 알려준다.
	PendingUserId = UserId;
	SetRequestPending(true);
	SetStatusMessage(ConnectingStatusText);
	GameInstance->ConnectAndRegister(UserId, Password);
}

UUEAccountSubsystem* UUELoginWidget::GetAccountSubsystem() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UUEAccountSubsystem>() : nullptr;
}

FText UUELoginWidget::GetRegistrationResultMessage(EUELocalAccountResult Result) const
{
	switch (Result)
	{
	case EUELocalAccountResult::EmptyNickname:
		return NicknameRequiredStatusText;
	case EUELocalAccountResult::InvalidNicknameLength:
		return InvalidNicknameLengthStatusText;
	case EUELocalAccountResult::EmptyUserId:
		return UserIdRequiredStatusText;
	case EUELocalAccountResult::InvalidUserIdLength:
		return InvalidUserIdLengthStatusText;
	case EUELocalAccountResult::InvalidUserIdCharacters:
		return InvalidUserIdCharactersStatusText;
	case EUELocalAccountResult::InvalidPasswordLength:
		return InvalidPasswordLengthStatusText;
	case EUELocalAccountResult::PasswordConfirmationMismatch:
		return PasswordMismatchStatusText;
	case EUELocalAccountResult::AccountAlreadyExists:
		return AccountAlreadyExistsStatusText;
	case EUELocalAccountResult::SaveFailed:
		return SaveFailedStatusText;
	default:
		return InvalidFieldsStatusText;
	}
}
