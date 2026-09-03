#include "UECharacterNameWidget.h"

#include "../../System/UEGameInstance.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void UUECharacterNameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);
	if (UUEGameInstance* GameInstance = GetHHVGameInstance())
	{
		GameInstance->OnNicknameChecked.AddUniqueDynamic(this, &ThisClass::HandleNicknameChecked);
	}
	SetRequestPending(false);
	SetStatus(ReadyMessage);
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
	if (UUEGameInstance* GameInstance = GetHHVGameInstance())
	{
		GameInstance->OnNicknameChecked.RemoveDynamic(this, &ThisClass::HandleNicknameChecked);
	}
	Super::NativeDestruct();
}

UUEGameInstance* UUECharacterNameWidget::GetHHVGameInstance() const
{
	return Cast<UUEGameInstance>(GetGameInstance());
}

void UUECharacterNameWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

void UUECharacterNameWidget::SetRequestPending(bool bPending)
{
	bRequestPending = bPending;
	if (ConfirmButton)
	{
		ConfirmButton->SetIsEnabled(!bPending);
	}
}

void UUECharacterNameWidget::HandleConfirmClicked()
{
	if (bRequestPending)
	{
		return;
	}

	const FString CharacterName = CharacterNameInput->GetText().ToString().TrimStartAndEnd();
	if (CharacterName.IsEmpty())
	{
		SetStatus(NameRequiredMessage);
		return;
	}

	// 서버 왕복이라 여기서 결과가 나오지 않는다. HandleNicknameChecked 에서 잇는다.
	// 길이·문자 검사도 서버가 한다 — 규칙이 두 군데 있으면 한쪽만 바뀐다.
	UUEGameInstance* GameInstance = GetHHVGameInstance();
	if (!GameInstance)
	{
		SetStatus(NameUnavailableMessage);
		return;
	}

	PendingName = CharacterName;
	SetRequestPending(true);
	SetStatus(CheckingMessage);
	GameInstance->RequestCheckNickname(CharacterName);
}

void UUECharacterNameWidget::HandleNicknameChecked(bool bOk, const FString& Message)
{
	SetRequestPending(false);

	if (!bOk)
	{
		// 서버가 준 사유를 그대로 보여준다 (중복인지 형식인지 서버가 안다).
		SetStatus(Message.IsEmpty() ? NameUnavailableMessage : FText::FromString(Message));
		return;
	}

	// 여기서 통과해도 생성 때 서버가 다시 본다. 그 사이에 남이 같은 이름을
	// 가져갈 수 있고, 마지막 심판은 DB 의 유니크 키다.
	OnNameConfirmed.Broadcast(PendingName);
}

void UUECharacterNameWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}
