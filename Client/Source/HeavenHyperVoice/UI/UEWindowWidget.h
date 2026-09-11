#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UEWindowWidget.generated.h"

/** 창 활성화만 담당한다. 배치와 외형은 자식 위젯 블루프린트에서 편집한다. */
UCLASS(Abstract, Blueprintable)
class HEAVENHYPERVOICE_API UUEWindowWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void RemoveFromParent() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Window")
	bool bActivateOnOpen = true;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& Event) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
};
