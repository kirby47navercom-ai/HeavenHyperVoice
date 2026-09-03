#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UECharacterNameWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUECharacterNameConfirmedSignature, const FString&, CharacterName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUECharacterNameBackRequestedSignature);

/**
 * 캐릭터 생성의 첫 단계에서 이름 하나만 입력받는 WBP 기반 위젯이다.
 *
 * 확인을 누르면 곧장 넘어가지 않고 서버에 이름이 비었는지 먼저 묻는다.
 * 생성은 이름 -> 커마 -> 스타터 순서라, 중복을 생성 시점에 알면 커마를 통째로
 * 다시 거쳐야 하기 때문이다. 서버가 통과시킨 뒤에야 OnNameConfirmed 가 나간다.
 */
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

	// 서버에 묻는 동안 보여준다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Name|Text")
	FText CheckingMessage;

	// 서버가 사유 없이 거절했을 때만 쓴다. 보통은 서버 문구를 그대로 보여준다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Name|Text")
	FText NameUnavailableMessage;

private:
	class UUEGameInstance* GetHHVGameInstance() const;
	void SetStatus(const FText& Message);
	void SetRequestPending(bool bPending);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleNicknameChecked(bool bOk, const FString& Message);

	// 서버에 물어본 이름. 응답에는 이름이 실려 오지 않아서 들고 있는다.
	FString PendingName;

	// 응답을 기다리는 중이다. 연타로 두 번째 요청이 나가면 서버가 연결을 끊는다
	// (LoginHandler 의 Busy 단계).
	bool bRequestPending = false;
};
