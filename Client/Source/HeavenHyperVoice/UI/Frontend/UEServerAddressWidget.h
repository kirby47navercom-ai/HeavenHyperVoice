#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UEServerAddressWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUEServerAddressConfirmedSignature, const FString&, ServerAddress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUEServerAddressBackRequestedSignature);

/** 로그인 전에 접속할 서버 IP 하나를 입력받는 WBP 기반 위젯이다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUEServerAddressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Frontend|Server Address|Event")
	FUEServerAddressConfirmedSignature OnServerAddressConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Frontend|Server Address|Event")
	FUEServerAddressBackRequestedSignature OnBackRequested;

	/** 이전 화면 이동에서 이미 입력한 주소가 있으면 입력칸에 다시 표시한다. */
	UFUNCTION(BlueprintCallable, Category = "Frontend|Server Address")
	void SetInitialServerAddress(const FString& ServerAddress);

	/** 점으로 구분된 IPv4 주소인지 검사한다. */
	UFUNCTION(BlueprintPure, Category = "Frontend|Server Address")
	static bool IsValidServerAddress(const FString& ServerAddress);

	// 안내 문구는 WBP 기본값에서 바꿀 수 있게 코드 문자열을 두지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frontend|Server Address|Text")
	FText ReadyMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frontend|Server Address|Text")
	FText InvalidAddressMessage;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 배치와 스타일은 WBP_ServerAddress에서만 편집한다.
	// 원본 반응형 WBP의 내부 이름을 유지해 위젯 GUID를 안전하게 보존한다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> CharacterNameInput = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText = nullptr;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleBackClicked();
};
