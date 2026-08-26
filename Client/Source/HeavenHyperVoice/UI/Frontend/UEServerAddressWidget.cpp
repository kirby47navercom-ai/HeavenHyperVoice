#include "UEServerAddressWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void UUEServerAddressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);
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
	const FString ServerAddress = CharacterNameInput->GetText().ToString().TrimStartAndEnd();
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
