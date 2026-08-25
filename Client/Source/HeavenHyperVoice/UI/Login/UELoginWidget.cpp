#include "UELoginWidget.h"

#include "../../System/Account/UEAccountSubsystem.h"

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

	SetScreenMode(EUELoginScreenMode::Login);
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
	UUEAccountSubsystem* AccountSubsystem = GetAccountSubsystem();
	if (!AccountSubsystem)
	{
		SetStatusMessage(StorageUnavailableStatusText);
		return;
	}

	const FString UserId = IdInputBox->GetText().ToString().TrimStartAndEnd();
	const EUELocalAccountResult Result = AccountSubsystem->CheckUserIdAvailability(UserId);
	if (Result == EUELocalAccountResult::Success)
	{
		bUserIdDuplicateChecked = true;
		DuplicateCheckedUserId = UserId.ToLower();
		SetStatusMessage(UserIdAvailableStatusText);
		return;
	}

	ResetDuplicateCheck();
	SetStatusMessage(GetRegistrationResultMessage(Result));
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

void UUELoginWidget::HandleLogin()
{
	UUEAccountSubsystem* AccountSubsystem = GetAccountSubsystem();
	if (!AccountSubsystem)
	{
		SetStatusMessage(LoginFailedStatusText);
		return;
	}

	const FString UserId = IdInputBox->GetText().ToString().TrimStartAndEnd();
	const FString Password = PasswordInputBox->GetText().ToString();
	FString Nickname;
	const EUELocalAccountResult Result = AccountSubsystem->LoginAccount(UserId, Password, Nickname);
	if (Result != EUELocalAccountResult::Success)
	{
		// 아이디 존재 여부를 노출하지 않도록 실패 이유를 하나의 문구로 합친다.
		SetStatusMessage(LoginFailedStatusText);
		return;
	}

	SetStatusMessage(LoginSucceededStatusText);
	OnLoginSucceeded.Broadcast(UserId, Nickname);
}

void UUELoginWidget::HandleRegistration()
{
	const FString UserId = IdInputBox->GetText().ToString().TrimStartAndEnd();
	if (!bUserIdDuplicateChecked || DuplicateCheckedUserId != UserId.ToLower())
	{
		SetStatusMessage(DuplicateCheckRequiredStatusText);
		return;
	}

	UUEAccountSubsystem* AccountSubsystem = GetAccountSubsystem();
	if (!AccountSubsystem)
	{
		SetStatusMessage(StorageUnavailableStatusText);
		return;
	}

	const FString Nickname = NicknameInputBox->GetText().ToString();
	const FString Password = PasswordInputBox->GetText().ToString();
	const FString Confirmation = ConfirmPasswordInputBox->GetText().ToString();
	const EUELocalAccountResult Result = AccountSubsystem->RegisterAccount(Nickname, UserId, Password, Confirmation);
	if (Result != EUELocalAccountResult::Success)
	{
		// 아이디가 그대로라면 비밀번호나 닉네임 수정 때문에 중복 확인을 반복하지 않는다.
		if (Result == EUELocalAccountResult::AccountAlreadyExists
			|| Result == EUELocalAccountResult::EmptyUserId
			|| Result == EUELocalAccountResult::InvalidUserIdLength
			|| Result == EUELocalAccountResult::InvalidUserIdCharacters)
		{
			ResetDuplicateCheck();
		}
		SetStatusMessage(GetRegistrationResultMessage(Result));
		return;
	}

	OnAccountRegistered.Broadcast(UserId, Nickname.TrimStartAndEnd());
	PasswordInputBox->SetText(FText::GetEmpty());
	ConfirmPasswordInputBox->SetText(FText::GetEmpty());
	NicknameInputBox->SetText(FText::GetEmpty());
	ScreenMode = EUELoginScreenMode::Login;
	ResetDuplicateCheck();
	RefreshScreenMode();
	SetStatusMessage(RegistrationSucceededStatusText);
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
