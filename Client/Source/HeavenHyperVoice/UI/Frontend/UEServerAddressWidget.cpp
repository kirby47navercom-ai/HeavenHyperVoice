#include "UEServerAddressWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UUEServerAddressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);
	CharacterNameInput->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleAddressCommitted);
	if (LocalAddressButton)
	{
		LocalAddressButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLocalAddressClicked);
	}
	SetStatus(ReadyMessage, EUEFrontendStatusKind::Info);
	FocusRings.Collect(this);
	if (PanelIn)
	{
		PlayAnimation(PanelIn);
	}
	CharacterNameInput->SetKeyboardFocus();
}

void UUEServerAddressWidget::NativeDestruct()
{
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleBackClicked);
	}
	if (CharacterNameInput)
	{
		CharacterNameInput->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleAddressCommitted);
	}
	if (LocalAddressButton)
	{
		LocalAddressButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleLocalAddressClicked);
	}

	Super::NativeDestruct();
}

void UUEServerAddressWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	FocusRings.Refresh();
}

void UUEServerAddressWidget::SetInitialServerAddress(const FString& ServerAddress)
{
	if (CharacterNameInput)
	{
		CharacterNameInput->SetText(FText::FromString(ServerAddress));
	}
}

bool UUEServerAddressWidget::IsValidServerAddress(const FString& ServerAddress)
{
	TArray<FString> Octets;
	ServerAddress.TrimStartAndEnd().ParseIntoArray(Octets, TEXT("."), false);
	if (Octets.Num() != 4)
	{
		return false;
	}

	for (const FString& Octet : Octets)
	{
		if (Octet.IsEmpty() || Octet.Len() > 3)
		{
			return false;
		}

		for (const TCHAR Character : Octet)
		{
			if (!FChar::IsDigit(Character))
			{
				return false;
			}
		}

		const int32 Value = FCString::Atoi(*Octet);
		if (Value < 0 || Value > 255)
		{
			return false;
		}
	}

	return true;
}

void UUEServerAddressWidget::HandleConfirmClicked()
{
	ConfirmAddress();
}

void UUEServerAddressWidget::HandleAddressCommitted(const FText& Text,
	ETextCommit::Type CommitMethod)
{
	// 포커스가 빠지는 것만으로 넘어가면 안 된다. 엔터일 때만 확인으로 본다.
	if (CommitMethod == ETextCommit::OnEnter)
	{
		ConfirmAddress();
	}
}

void UUEServerAddressWidget::ConfirmAddress()
{
	FString ServerAddress = CharacterNameInput->GetText().ToString().TrimStartAndEnd();

	// 비워 두고 엔터를 치면 기본 주소로 붙는다. 입력칸에도 채워 넣어 어디로
	// 붙는지 보이게 한다.
	if (ServerAddress.IsEmpty())
	{
		ServerAddress = DefaultServerAddress;
		CharacterNameInput->SetText(FText::FromString(ServerAddress));
	}

	if (!IsValidServerAddress(ServerAddress))
	{
		SetStatus(InvalidAddressMessage, EUEFrontendStatusKind::Error);
		CharacterNameInput->SetKeyboardFocus();
		return;
	}

	OnServerAddressConfirmed.Broadcast(ServerAddress);
}

void UUEServerAddressWidget::SetStatus(const FText& Message, EUEFrontendStatusKind Kind)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
	UEFrontendStatus::Apply(StatusStyles, Kind, StatusText, StatusIcon, nullptr);

	// 입력 중에 스타일을 매번 다시 넣지 않도록 오류 여부가 바뀔 때만 바꾼다.
	const bool bError = Kind == EUEFrontendStatusKind::Error;
	if (bUseInputErrorStyle && CharacterNameInput && bError != bInputShowsError)
	{
		CharacterNameInput->SetWidgetStyle(bError ? InputErrorStyle : InputStyle);
		bInputShowsError = bError;
	}
	OnStatusChanged(Kind);
}

void UUEServerAddressWidget::HandleLocalAddressClicked()
{
	CharacterNameInput->SetText(FText::FromString(DefaultServerAddress));
	SetStatus(ReadyMessage, EUEFrontendStatusKind::Info);
	CharacterNameInput->SetKeyboardFocus();
}

void UUEServerAddressWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}
