#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UEPhotoModeWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;

/** Layout and decoration live in WBP_PhotoMode. This class only binds controls and values. */
UCLASS(Abstract, Blueprintable)
class HEAVENHYPERVOICE_API UUEPhotoModeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void UpdateZoom(float Value, float Magnification);
	void SetStatus(const FText& Text);
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> CaptureButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ExitButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ZoomInButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ZoomOutButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> ZoomSlider;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ZoomText;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusText;
private:
	UFUNCTION() void Capture();
	UFUNCTION() void Exit();
	UFUNCTION() void ZoomIn();
	UFUNCTION() void ZoomOut();
	UFUNCTION() void ZoomChanged(float Value);
};
