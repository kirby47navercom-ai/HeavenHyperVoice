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

	UFUNCTION()
	void HandleStarterBackRequested();

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AUEHHVCustomizationPreviewActor> PreviewActor = nullptr;

};
