#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UEPalworldCustomizationPlayerController.generated.h"

class AUEPalworldCustomizationPreviewActor;
class UUEPalworldCustomizationWidget;

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEPalworldCustomizationPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AUEPalworldCustomizationPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Palworld")
	TSubclassOf<UUEPalworldCustomizationWidget> CustomizationWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Palworld")
	int32 WidgetZOrder = 50;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUEPalworldCustomizationWidget> CustomizationWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AUEPalworldCustomizationPreviewActor> PreviewActor = nullptr;

	bool bDraggingPreview = false;
	bool bPanningPreview = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;

	bool IsMouseOverPreviewArea(const FVector2D& MousePosition) const;
	void HandlePreviewMouseDrag();
	void HandlePreviewZoomIn();
	void HandlePreviewZoomOut();
};
