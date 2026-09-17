#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UETitleWidget.h"
#include "UEFrontendPlayerController.generated.h"

class UUECharacterSelectionWidget;
class UUECharacterNameWidget;
class UUELoginWidget;
class UUEServerAddressWidget;
class UUserWidget;
class UWorld;

/** 타이틀부터 로비까지 로컬 화면 흐름을 한 곳에서 관리하는 컨트롤러다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEFrontendPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// 실제 화면 클래스는 BP_FrontendPlayerController 기본값에서 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frontend|Widgets")
	TSubclassOf<UUETitleWidget> TitleWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frontend|Widgets")
	TSubclassOf<UUELoginWidget> LoginWidgetClass;

	// 새 화면 클래스도 BP_FrontendPlayerController 기본값에서 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frontend|Widgets")
	TSubclassOf<UUEServerAddressWidget> ServerAddressWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frontend|Widgets")
	TSubclassOf<UUECharacterSelectionWidget> LobbyWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frontend|Widgets")
	TSubclassOf<UUECharacterNameWidget> CharacterNameWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frontend|Travel")
	TSoftObjectPtr<UWorld> CustomizationLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frontend|Widgets")
	int32 WidgetZOrder = 100;

	// 타이틀에서 서버 화면으로 줌아웃할 때, 줌이 이만큼 진행된 뒤에 서버 패널을 붙인다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frontend|Widgets", meta = (ClampMin = "0.0", Units = "s"))
	float MenuRevealDelay = 0.95f;

private:
	void ShowTitle();
	void ShowServerAddress();
	void ShowLogin();
	void ShowLobby();
	void ShowCharacterName();
	void ReplaceCurrentWidget(UUserWidget* NewWidget, float RevealDelay = 0.0f);
	void ClearCurrentWidget();

	// 타이틀 위젯은 서버·로그인 화면 뒤 배경으로도 쓰여서 CurrentWidget 과 따로 관리한다.
	UUETitleWidget* EnsureTitleLayer(EUEFrontendShot InitialShot);
	void RemoveTitleLayer();
	void ShowOverTitleLayer(UUserWidget* NewWidget);
	void ApplyFrontendInputMode(UUserWidget* FocusWidget);

	UFUNCTION()
	void HandleTitleContinueRequested();

	UFUNCTION()
	void HandleServerAddressConfirmed(const FString& ServerAddress);

	UFUNCTION()
	void HandleServerAddressBackRequested();

	UFUNCTION()
	void HandleLoginSucceeded(const FString& UserId, const FString& Nickname);

	UFUNCTION()
	void HandleLoginBackRequested();

	UFUNCTION()
	void HandleCharacterCreationRequested(int32 SlotIndex);

	UFUNCTION()
	void HandleCharacterNameConfirmed(const FString& CharacterName);

	UFUNCTION()
	void HandleCharacterNameBackRequested();

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUETitleWidget> TitleLayer = nullptr;

	FTimerHandle RevealTimer;
};
