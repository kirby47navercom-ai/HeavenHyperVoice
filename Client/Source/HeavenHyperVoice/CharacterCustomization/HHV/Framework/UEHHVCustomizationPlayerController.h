#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UEHHVCustomizationPlayerController.generated.h"

class AUEHHVCustomizationPreviewActor;
class UUEHHVCustomizationWidget;
class UUEPokemonSpeciesData;
class UUEStarterPokemonWidget;
class UUserWidget;
class UWorld;

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEHHVCustomizationPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AUEHHVCustomizationPlayerController();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization")
	TSubclassOf<UUEHHVCustomizationWidget> CustomizationWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization")
	TSubclassOf<UUEStarterPokemonWidget> StarterPokemonWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Travel")
	TSoftObjectPtr<UWorld> LobbyLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Text")
	FText SaveFailedMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization")
	int32 WidgetZOrder = 50;

private:
	void ShowCustomization();
	void ShowStarterPokemon();
	void ReplaceCurrentWidget(UUserWidget* NewWidget);
	void ApplyInputMode(UUserWidget* FocusWidget);
	void ReturnToLobby();

	UFUNCTION()
	void HandleCustomizationConfirmed();

	UFUNCTION()
	void HandleCustomizationBackRequested();

	UFUNCTION()
	void HandleStarterConfirmed(UUEPokemonSpeciesData* StarterPokemon);

	// 캐릭터 생성은 서버 왕복이다. 응답이 와야 로비로 돌아간다 — 먼저 돌아가면
	// 아직 목록에 없는 캐릭터를 그리려 하고, 실패했을 때 알릴 자리도 없다.
	UFUNCTION()
	void HandleServerCreateCompleted(bool bOk, const FString& Message);

	UFUNCTION()
	void HandleStarterBackRequested();

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AUEHHVCustomizationPreviewActor> PreviewActor = nullptr;

};
