#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UEHHVCustomizationPlayerController.generated.h"

class AUEHHVCustomizationPreviewActor;
class UUEHHVCustomizationWidget;

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEHHVCustomizationPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AUEHHVCustomizationPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization")
	TSubclassOf<UUEHHVCustomizationWidget> CustomizationWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization")
	int32 WidgetZOrder = 50;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUEHHVCustomizationWidget> CustomizationWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AUEHHVCustomizationPreviewActor> PreviewActor = nullptr;

	bool bDraggingPreview = false;
	bool bPanningPreview = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;

	bool IsMouseOverPreviewArea(const FVector2D& MousePosition) const;
	void HandlePreviewMouseDrag();
	void HandlePreviewZoomIn();
	void HandlePreviewZoomOut();
};
