#include "UECharacterNameWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void UUECharacterNameWidget::NativeConstruct()
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

void UUECharacterNameWidget::NativeDestruct()
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

void UUECharacterNameWidget::HandleConfirmClicked()
{
	const FString CharacterName = CharacterNameInput->GetText().ToString().TrimStartAndEnd();
	if (CharacterName.IsEmpty())
	{
		if (StatusText)
		{
			StatusText->SetText(NameRequiredMessage);
		}
		return;
	}

	OnNameConfirmed.Broadcast(CharacterName);
}

void UUECharacterNameWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}
