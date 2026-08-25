#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UECharacterNameWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUECharacterNameConfirmedSignature, const FString&, CharacterName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUECharacterNameBackRequestedSignature);

/** 캐릭터 생성의 첫 단계에서 이름 하나만 입력받는 WBP 기반 위젯이다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUECharacterNameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Character Name|Event")
	FUECharacterNameConfirmedSignature OnNameConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Character Name|Event")
	FUECharacterNameBackRequestedSignature OnBackRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> CharacterNameInput = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Name|Text")
	FText ReadyMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Name|Text")
	FText NameRequiredMessage;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleBackClicked();
};
