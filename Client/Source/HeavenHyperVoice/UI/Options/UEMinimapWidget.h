#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UEMinimapWidget.generated.h"

class UImage;
class UTextBlock;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/** WBP_Minimap owns the circular layout. Capture and heading are local presentation only. */
UCLASS(Abstract, Blueprintable)
class HEAVENHYPERVOICE_API UUEMinimapWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> MapImage;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> PlayerArrow;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> ViewArrow;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> LocationText;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap", meta=(ClampMin="500"))
	float ViewWidth = 4200.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap", meta=(ClampMin="500"))
	float CaptureHeight = 8000.f;
	UPROPERTY(EditDefaultsOnly, Category="Minimap", meta=(ClampMin="0.05"))
	float UpdateInterval = .15f;
private:
	UPROPERTY(Transient) TObjectPtr<AActor> CaptureActor;
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Target;
	float Elapsed = 1.f;
};
