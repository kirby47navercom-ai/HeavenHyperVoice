#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UECustomizationPlayerController.generated.h"

class AUECustomizationPreviewActor;
class UUECustomizationWidget;

/** Connects the standalone customization scene, preview actor, and UMG widget. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUECustomizationPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AUECustomizationPlayerController();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization")
	TSubclassOf<UUECustomizationWidget> CustomizationWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization")
	int32 WidgetZOrder = 50;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUECustomizationWidget> CustomizationWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AUECustomizationPreviewActor> PreviewActor = nullptr;
};
