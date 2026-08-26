#include "UEServerAddressWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void UUEServerAddressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);
	CharacterNameInput->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleAddressCommitted);
	if (StatusText)
	{
		StatusText->SetText(ReadyMessage);
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

	Super::NativeDestruct();
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
		if (StatusText)
		{
			StatusText->SetText(InvalidAddressMessage);
		}
		CharacterNameInput->SetKeyboardFocus();
		return;
	}

	OnServerAddressConfirmed.Broadcast(ServerAddress);
}

void UUEServerAddressWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}
