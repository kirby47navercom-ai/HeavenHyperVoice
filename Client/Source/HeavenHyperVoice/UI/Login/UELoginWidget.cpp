#include "UELoginWidget.h"

// 로컬 계정 시스템은 더 이상 로그인 경로에서 쓰지 않는다. 인증은 LoginServer 가
// 한다. 헤더는 GetRegistrationResultMessage 가 아직 참조해서 남겨둔다.
#include "../../System/Account/UEAccountSubsystem.h"
#include "../../System/UEGameInstance.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
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
	if (LoginTabButton)
	{
		LoginTabButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLoginTabClicked);
	}
	PrimaryActionButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePrimaryActionClicked);
	IdInputBox->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleUserIdChanged);
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);

	if (UUEGameInstance* GameInstance = GetHHVGameInstance())
	{
		GameInstance->OnLoginCompleted.AddUniqueDynamic(this, &ThisClass::HandleServerLoginCompleted);
		GameInstance->OnRegisterCompleted.AddUniqueDynamic(this, &ThisClass::HandleServerRegisterCompleted);
		GameInstance->OnServerDisconnected.AddUniqueDynamic(this, &ThisClass::HandleServerDisconnected);

		if (ServerAddressText)
		{
			ServerAddressText->SetText(FText::FromString(GameInstance->GetServerAddress()));
		}
	}

	SetScreenMode(EUELoginScreenMode::Login);
	SetRequestPending(false);
	FocusRings.Collect(this);

	if (PanelIn)
	{
		PlayAnimation(PanelIn);
	}
	if (StatusWaveLoop)
	{
		// 반복 횟수 0 은 끝없이 반복이다.
		PlayAnimation(StatusWaveLoop, 0.0f, 0);
	}
}

void UUELoginWidget::NativeDestruct()
{
	if (RegisterTabButton)
	{
		RegisterTabButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRegisterTabClicked);
	}
	if (LoginTabButton)
	{
		LoginTabButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleLoginTabClicked);
	}
	if (PrimaryActionButton)
	{
		PrimaryActionButton->OnClicked.RemoveDynamic(this, &ThisClass::HandlePrimaryActionClicked);
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

void UUELoginWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	FocusRings.Refresh();
}

void UUELoginWidget::SetScreenMode(EUELoginScreenMode NewMode)
{
	ScreenMode = NewMode;
	RefreshScreenMode();
	if (ScreenMode == EUELoginScreenMode::Register)
	{
		// 로그인에서 쳐 둔 아이디가 있으면 그 형식부터 알려준다.
		ShowUserIdFormatGuide(IdInputBox->GetText().ToString().TrimStartAndEnd());
		return;
	}
	SetStatusMessage(ReadyStatusText);
}

void UUELoginWidget::SetStatusMessage(const FText& Message, EUEFrontendStatusKind Kind)
{
	SetStatus(Message, Kind, nullptr);
}

void UUELoginWidget::SetStatus(const FText& Message, EUEFrontendStatusKind Kind, UEditableTextBox* ErrorInput)
{
	if (StatusBlock)
	{
		StatusBlock->SetText(Message);
	}
	UEFrontendStatus::Apply(StatusStyles, Kind, StatusBlock, StatusIcon, StatusWave);

	if (bUseInputErrorStyle)
	{
		const bool bError = Kind == EUEFrontendStatusKind::Error;
		for (UEditableTextBox* Input : {IdInputBox.Get(), PasswordInputBox.Get(), ConfirmPasswordInputBox.Get()})
		{
			MarkInputError(Input, bError && Input == ErrorInput);
		}
	}
	OnStatusChanged(Kind);
}

void UUELoginWidget::MarkInputError(UEditableTextBox* Input, bool bError)
{
	if (!Input || bError == InputsShowingError.Contains(Input))
	{
		return;
	}
	// 입력 중에 스타일을 매번 다시 넣지 않도록 오류 여부가 바뀔 때만 넣는다.
	Input->SetWidgetStyle(bError ? InputErrorStyle : InputStyle);
	if (bError)
	{
		InputsShowingError.Add(Input);
	}
	else
	{
		InputsShowingError.Remove(Input);
	}
}

void UUELoginWidget::RefreshScreenMode()
{
	const bool bIsRegisterMode = ScreenMode == EUELoginScreenMode::Register;
	if (SubtitleBlock)
	{
		SubtitleBlock->SetText(bIsRegisterMode ? RegisterSubtitleText : LoginSubtitleText);
	}
	if (NicknameRow)
	{
		// 닉네임은 캐릭터 속성이라 가입에 싣지 않는다. WBP 에서 지울 때까지 접어 둔다.
		NicknameRow->SetVisibility(ESlateVisibility::Collapsed);
	}
	ConfirmPasswordRow->SetVisibility(bIsRegisterMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RegisterTabButton->SetVisibility(bIsRegisterMode && !LoginTabButton ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	PrimaryActionLabel->SetText(bIsRegisterMode ? CreateAccountButtonText : LoginButtonText);
	RefreshTabs();
	OnScreenModeChanged(ScreenMode);
}

void UUELoginWidget::RefreshTabs()
{
	if (!bUseTabStyles || !LoginTabButton)
	{
		return;
	}

	const bool bIsRegisterMode = ScreenMode == EUELoginScreenMode::Register;
	LoginTabButton->SetStyle(bIsRegisterMode ? TabStyle : SelectedTabStyle);
	RegisterTabButton->SetStyle(bIsRegisterMode ? SelectedTabStyle : TabStyle);
	if (LoginTabLabel)
	{
		LoginTabLabel->SetColorAndOpacity(FSlateColor(bIsRegisterMode ? TabTextColor : SelectedTabTextColor));
	}
	if (RegisterTabLabel)
	{
		RegisterTabLabel->SetColorAndOpacity(FSlateColor(bIsRegisterMode ? SelectedTabTextColor : TabTextColor));
	}
	if (LoginTabTick)
	{
		LoginTabTick->SetVisibility(bIsRegisterMode ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
	if (RegisterTabTick)
	{
		RegisterTabTick->SetVisibility(bIsRegisterMode ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UUELoginWidget::HandleRegisterTabClicked()
{
	SetScreenMode(EUELoginScreenMode::Register);
}

void UUELoginWidget::HandleLoginTabClicked()
{
	SetScreenMode(EUELoginScreenMode::Login);
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

void UUELoginWidget::ShowUserIdFormatGuide(const FString& UserId)
{
	// 서버에는 "이 아이디 쓸 수 있나" 를 묻는 메시지가 없다. 중복이면 가입 응답이
	// "이미 사용 중인 아이디입니다" 로 알려준다. 그래서 입력하는 동안에는 서버가 어차피
	// 거절할 형식만 알려준다. 규칙은 LoginCodec.h 의 isValidUsername 과 같다:
	// 영문/숫자/밑줄, 3~32자.
	if (UserId.IsEmpty())
	{
		SetStatusMessage(RegisterReadyStatusText);
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
			SetStatus(InvalidUserIdCharactersStatusText, EUEFrontendStatusKind::Error, IdInputBox);
			return;
		}
	}

	// 길이는 아직 치는 중일 수 있어 오류가 아니라 안내로 보여준다.
	if (UserId.Len() < 3 || UserId.Len() > 32)
	{
		SetStatusMessage(InvalidUserIdLengthStatusText);
		return;
	}

	SetStatusMessage(UserIdFormatOkStatusText, EUEFrontendStatusKind::Success);
}

void UUELoginWidget::HandleBackClicked()
{
	if (ScreenMode == EUELoginScreenMode::Register && !LoginTabButton)
	{
		SetScreenMode(EUELoginScreenMode::Login);
		return;
	}

	OnBackRequested.Broadcast();
}

void UUELoginWidget::HandleUserIdChanged(const FText& NewText)
{
	// 응답을 기다리는 동안 문구를 덮으면 연결 중 표시가 사라진다.
	if (ScreenMode == EUELoginScreenMode::Register && !bRequestPending)
	{
		ShowUserIdFormatGuide(NewText.ToString().TrimStartAndEnd());
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
}

void UUELoginWidget::HandleLogin()
{
	UUEGameInstance* GameInstance = GetHHVGameInstance();
	if (!GameInstance)
	{
		SetStatusMessage(LoginFailedStatusText, EUEFrontendStatusKind::Error);
		return;
	}

	const FString UserId = IdInputBox->GetText().ToString().TrimStartAndEnd();
	const FString Password = PasswordInputBox->GetText().ToString();
	if (UserId.IsEmpty() || Password.IsEmpty())
	{
		SetStatus(InvalidFieldsStatusText, EUEFrontendStatusKind::Error, UserId.IsEmpty() ? IdInputBox : PasswordInputBox);
		return;
	}

	// 서버 왕복이라 여기서 결과가 나오지 않는다. HandleServerLoginCompleted 에서 잇는다.
	PendingUserId = UserId;
	SetRequestPending(true);
	SetStatusMessage(ConnectingStatusText, EUEFrontendStatusKind::Pending);
	GameInstance->ConnectAndLogin(UserId, Password);
}

void UUELoginWidget::HandleServerLoginCompleted(bool bOk, const FString& Message)
{
	SetRequestPending(false);

	if (!bOk)
	{
		// 서버가 준 사유를 그대로 보여준다. 아이디 존재 여부는 서버가 이미
		// 하나의 문구로 합쳐서 보낸다.
		SetStatus(Message.IsEmpty() ? LoginFailedStatusText : FText::FromString(Message), EUEFrontendStatusKind::Error, PasswordInputBox);
		return;
	}

	SetStatusMessage(LoginSucceededStatusText, EUEFrontendStatusKind::Success);
	PasswordInputBox->SetText(FText::GetEmpty());

	// 닉네임은 캐릭터 속성이라 로그인 응답에 없다. 캐릭터를 고를 때 정해진다.
	OnLoginSucceeded.Broadcast(PendingUserId, FString());
}

void UUELoginWidget::HandleServerRegisterCompleted(bool bOk, const FString& Message)
{
	SetRequestPending(false);

	if (!bOk)
	{
		// 가입 거절은 대부분 아이디 문제(중복·형식)다.
		SetStatus(Message.IsEmpty() ? SaveFailedStatusText : FText::FromString(Message), EUEFrontendStatusKind::Error, IdInputBox);
		return;
	}

	OnAccountRegistered.Broadcast(PendingUserId, FString());
	PasswordInputBox->SetText(FText::GetEmpty());
	ConfirmPasswordInputBox->SetText(FText::GetEmpty());
	ScreenMode = EUELoginScreenMode::Login;
	RefreshScreenMode();
	SetStatusMessage(Message.IsEmpty() ? RegistrationSucceededStatusText : FText::FromString(Message), EUEFrontendStatusKind::Success);
}

void UUELoginWidget::HandleServerDisconnected(bool bOk, const FString& Message)
{
	// 요청 도중 끊긴 경우다. 버튼을 풀어 다시 시도할 수 있게 한다.
	SetRequestPending(false);
	SetStatusMessage(Message.IsEmpty() ? LoginFailedStatusText : FText::FromString(Message), EUEFrontendStatusKind::Error);
}

void UUELoginWidget::HandleRegistration()
{
	UUEGameInstance* GameInstance = GetHHVGameInstance();
	if (!GameInstance)
	{
		SetStatusMessage(StorageUnavailableStatusText, EUEFrontendStatusKind::Error);
		return;
	}

	const FString UserId = IdInputBox->GetText().ToString().TrimStartAndEnd();
	const FString Password = PasswordInputBox->GetText().ToString();
	const FString Confirmation = ConfirmPasswordInputBox->GetText().ToString();

	// 비밀번호 확인은 서버에 보낼 값이 아니라 입력 실수를 잡는 것이라 여기서 본다.
	if (Password != Confirmation)
	{
		SetStatus(PasswordMismatchStatusText, EUEFrontendStatusKind::Error, ConfirmPasswordInputBox);
		return;
	}
	if (UserId.IsEmpty() || Password.IsEmpty())
	{
		SetStatus(InvalidFieldsStatusText, EUEFrontendStatusKind::Error, UserId.IsEmpty() ? IdInputBox : PasswordInputBox);
		return;
	}

	// 닉네임은 계정이 아니라 캐릭터 속성이라 가입에 싣지 않는다. 캐릭터를 만들 때
	// 받는다. 아이디 중복은 서버가 가입 응답으로 알려준다.
	PendingUserId = UserId;
	SetRequestPending(true);
	SetStatusMessage(ConnectingStatusText, EUEFrontendStatusKind::Pending);
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
