#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UEFrontendPlayerController.generated.h"

class UUECharacterSelectionWidget;
class UUECharacterNameWidget;
class UUELoginWidget;
class UUEServerAddressWidget;
class UUETitleWidget;
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

private:
	void ShowTitle();
	void ShowServerAddress();
	void ShowLogin();
	void ShowLobby();
	void ShowCharacterName();
	void ReplaceCurrentWidget(UUserWidget* NewWidget);
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
};
