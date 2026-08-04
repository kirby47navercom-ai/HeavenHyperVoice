// Fill out your copyright notice in the Description page of Project Settings.

#include "UELoginWidget.h"

#include "../../System/Account/UEAccountSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace UELoginWidgetPrivate
{
	constexpr float RowGap = 8.0f;

	UTextBlock* MakeButtonLabel(UWidgetTree* WidgetTree, const FName Name, const FText& Text)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(Text);
		Label->SetFontSize(17);
		Label->SetJustification(ETextJustify::Center);
		return Label;
	}

	USizeBox* MakeFixedHeightRow(UWidgetTree* WidgetTree, const FName Name, float Height, UWidget* Child)
	{
		USizeBox* Row = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), Name);
		Row->SetHeightOverride(Height);
		Row->AddChild(Child);
		return Row;
	}
}

UUELoginWidget::UUELoginWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);

	TitleText = FText::FromString(TEXT("HEAVEN HYPER VOICE"));
	LoginSubtitleText = FText::FromString(TEXT("Client Login"));
	RegisterSubtitleText = FText::FromString(TEXT("Create Local Account"));
	LoginTabText = FText::FromString(TEXT("LOGIN"));
	RegisterTabText = FText::FromString(TEXT("SIGN UP"));
	NicknameHintText = FText::FromString(TEXT("Nickname"));
	IdHintText = FText::FromString(TEXT("ID (4-20 characters)"));
	PasswordHintText = FText::FromString(TEXT("Password (at least 6 characters)"));
	ConfirmPasswordHintText = FText::FromString(TEXT("Confirm password"));
	CheckDuplicateButtonText = FText::FromString(TEXT("CHECK"));
	LoginButtonText = FText::FromString(TEXT("LOGIN"));
	CreateAccountButtonText = FText::FromString(TEXT("CREATE ACCOUNT"));
	ReadyStatusText = FText::FromString(TEXT("Enter your ID and password."));
	RegisterReadyStatusText = FText::FromString(TEXT("Complete every field and check ID availability."));
	DuplicateCheckRequiredStatusText = FText::FromString(TEXT("Check ID availability first."));
	UserIdAvailableStatusText = FText::FromString(TEXT("This ID is available."));
	LoginFailedStatusText = FText::FromString(TEXT("ID or password is incorrect."));
	LoginSucceededStatusText = FText::FromString(TEXT("Login successful."));
	RegistrationSucceededStatusText = FText::FromString(TEXT("Account created. You can now log in."));
}

TSharedRef<SWidget> UUELoginWidget::RebuildWidget()
{
	// Build in C++ so the complete screen is visible without manual Blueprint wiring.
	// Blueprint children can still tune every exposed label and layout value.
	BuildLoginLayout();
	return Super::RebuildWidget();
}

void UUELoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	LoginTabButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLoginTabClicked);
	RegisterTabButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRegisterTabClicked);
	PrimaryActionButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePrimaryActionClicked);
	DuplicateCheckButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDuplicateCheckClicked);
	IdInputBox->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleUserIdChanged);

	SetScreenMode(EUELoginScreenMode::Login);
}

void UUELoginWidget::NativeDestruct()
{
	if (LoginTabButton)
	{
		LoginTabButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleLoginTabClicked);
	}
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

void UUELoginWidget::BuildLoginLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background_Fullscreen"));
	BackgroundBorder->SetBrushColor(FLinearColor(0.015f, 0.018f, 0.024f, 1.0f));
	UCanvasPanelSlot* BackgroundSlot = RootCanvas->AddChildToCanvas(BackgroundBorder);
	BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackgroundSlot->SetOffsets(FMargin(0.0f));

	UVerticalBox* Form = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Form_LoginRegister"));
	UCanvasPanelSlot* FormSlot = RootCanvas->AddChildToCanvas(Form);
	FormSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	FormSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	FormSlot->SetPosition(FVector2D::ZeroVector);
	FormSlot->SetSize(FVector2D(PanelWidth, 560.0f));
	FormSlot->SetZOrder(2);

	UTextBlock* TitleBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));
	TitleBlock->SetText(TitleText);
	TitleBlock->SetFontSize(34);
	TitleBlock->SetJustification(ETextJustify::Center);
	Form->AddChildToVerticalBox(TitleBlock)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

	SubtitleBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Subtitle"));
	SubtitleBlock->SetFontSize(16);
	SubtitleBlock->SetJustification(ETextJustify::Center);
	Form->AddChildToVerticalBox(SubtitleBlock)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Tabs_LoginRegister"));
	USizeBox* TabsRow = UELoginWidgetPrivate::MakeFixedHeightRow(WidgetTree, TEXT("Row_Tabs"), 40.0f, Tabs);
	Form->AddChildToVerticalBox(TabsRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	LoginTabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_LoginTab"));
	LoginTabButton->AddChild(UELoginWidgetPrivate::MakeButtonLabel(WidgetTree, TEXT("Text_LoginTab"), LoginTabText));
	Tabs->AddChildToHorizontalBox(LoginTabButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	RegisterTabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_RegisterTab"));
	RegisterTabButton->AddChild(UELoginWidgetPrivate::MakeButtonLabel(WidgetTree, TEXT("Text_RegisterTab"), RegisterTabText));
	Tabs->AddChildToHorizontalBox(RegisterTabButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	NicknameInputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input_Nickname"));
	NicknameInputBox->SetHintText(NicknameHintText);
	NicknameInputBox->SetForegroundColor(InputTextColor);
	NicknameRow = UELoginWidgetPrivate::MakeFixedHeightRow(WidgetTree, TEXT("Row_Nickname"), InputHeight, NicknameInputBox);
	Form->AddChildToVerticalBox(NicknameRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, UELoginWidgetPrivate::RowGap));

	UHorizontalBox* IdRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row_Id"));
	USizeBox* IdRowContainer = UELoginWidgetPrivate::MakeFixedHeightRow(WidgetTree, TEXT("Container_Id"), InputHeight, IdRow);
	Form->AddChildToVerticalBox(IdRowContainer)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, UELoginWidgetPrivate::RowGap));
	IdInputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input_Id"));
	IdInputBox->SetHintText(IdHintText);
	IdInputBox->SetForegroundColor(InputTextColor);
	IdRow->AddChildToHorizontalBox(IdInputBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	DuplicateCheckButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_CheckDuplicate"));
	DuplicateCheckButton->AddChild(UELoginWidgetPrivate::MakeButtonLabel(WidgetTree, TEXT("Text_CheckDuplicate"), CheckDuplicateButtonText));
	UHorizontalBoxSlot* DuplicateSlot = IdRow->AddChildToHorizontalBox(DuplicateCheckButton);
	DuplicateSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	DuplicateSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));

	PasswordInputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input_Password"));
	PasswordInputBox->SetHintText(PasswordHintText);
	PasswordInputBox->SetForegroundColor(InputTextColor);
	PasswordInputBox->SetIsPassword(true);
	USizeBox* PasswordRow = UELoginWidgetPrivate::MakeFixedHeightRow(WidgetTree, TEXT("Row_Password"), InputHeight, PasswordInputBox);
	Form->AddChildToVerticalBox(PasswordRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, UELoginWidgetPrivate::RowGap));

	ConfirmPasswordInputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input_ConfirmPassword"));
	ConfirmPasswordInputBox->SetHintText(ConfirmPasswordHintText);
	ConfirmPasswordInputBox->SetForegroundColor(InputTextColor);
	ConfirmPasswordInputBox->SetIsPassword(true);
	ConfirmPasswordRow = UELoginWidgetPrivate::MakeFixedHeightRow(WidgetTree, TEXT("Row_ConfirmPassword"), InputHeight, ConfirmPasswordInputBox);
	Form->AddChildToVerticalBox(ConfirmPasswordRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	PrimaryActionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_PrimaryAction"));
	PrimaryActionLabel = UELoginWidgetPrivate::MakeButtonLabel(WidgetTree, TEXT("Text_PrimaryAction"), LoginButtonText);
	PrimaryActionButton->AddChild(PrimaryActionLabel);
	USizeBox* PrimaryActionRow = UELoginWidgetPrivate::MakeFixedHeightRow(WidgetTree, TEXT("Row_PrimaryAction"), ButtonHeight, PrimaryActionButton);
	Form->AddChildToVerticalBox(PrimaryActionRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	StatusBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Status"));
	StatusBlock->SetAutoWrapText(true);
	StatusBlock->SetFontSize(15);
	StatusBlock->SetJustification(ETextJustify::Center);
	Form->AddChildToVerticalBox(StatusBlock);
}

void UUELoginWidget::RefreshScreenMode()
{
	const bool bIsRegisterMode = ScreenMode == EUELoginScreenMode::Register;
	SubtitleBlock->SetText(bIsRegisterMode ? RegisterSubtitleText : LoginSubtitleText);
	NicknameRow->SetVisibility(bIsRegisterMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	DuplicateCheckButton->SetVisibility(bIsRegisterMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ConfirmPasswordRow->SetVisibility(bIsRegisterMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	PrimaryActionLabel->SetText(bIsRegisterMode ? CreateAccountButtonText : LoginButtonText);
}

void UUELoginWidget::ResetDuplicateCheck()
{
	bUserIdDuplicateChecked = false;
	DuplicateCheckedUserId.Reset();
}

void UUELoginWidget::HandleLoginTabClicked()
{
	SetScreenMode(EUELoginScreenMode::Login);
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
		SetStatusMessage(FText::FromString(TEXT("Account storage is unavailable.")));
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

void UUELoginWidget::HandleUserIdChanged(const FText& NewText)
{
	// Any edit after a successful duplicate check invalidates that approval.
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
		// Missing IDs and wrong passwords deliberately share one message.
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
		SetStatusMessage(FText::FromString(TEXT("Account storage is unavailable.")));
		return;
	}

	const FString Nickname = NicknameInputBox->GetText().ToString();
	const FString Password = PasswordInputBox->GetText().ToString();
	const FString Confirmation = ConfirmPasswordInputBox->GetText().ToString();
	const EUELocalAccountResult Result = AccountSubsystem->RegisterAccount(Nickname, UserId, Password, Confirmation);
	if (Result != EUELocalAccountResult::Success)
	{
		// Password or nickname corrections do not invalidate an unchanged ID check.
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
		return FText::FromString(TEXT("Nickname is required."));
	case EUELocalAccountResult::InvalidNicknameLength:
		return FText::FromString(TEXT("Nickname must be 2-16 characters."));
	case EUELocalAccountResult::EmptyUserId:
		return FText::FromString(TEXT("ID is required."));
	case EUELocalAccountResult::InvalidUserIdLength:
		return FText::FromString(TEXT("ID must be 4-20 characters."));
	case EUELocalAccountResult::InvalidUserIdCharacters:
		return FText::FromString(TEXT("ID can use only letters, numbers, and _."));
	case EUELocalAccountResult::InvalidPasswordLength:
		return FText::FromString(TEXT("Password must be at least 6 characters."));
	case EUELocalAccountResult::PasswordConfirmationMismatch:
		return FText::FromString(TEXT("Passwords do not match."));
	case EUELocalAccountResult::AccountAlreadyExists:
		return FText::FromString(TEXT("This ID is already in use."));
	case EUELocalAccountResult::SaveFailed:
		return FText::FromString(TEXT("Could not save the account."));
	default:
		return FText::FromString(TEXT("Check the registration fields."));
	}
}
