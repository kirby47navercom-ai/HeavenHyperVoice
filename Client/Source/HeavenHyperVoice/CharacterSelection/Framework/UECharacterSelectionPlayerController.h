#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UECharacterSelectionPlayerController.generated.h"

class UUECharacterSelectionWidget;

/** 캐릭터 선택 레벨에서 디자이너 UMG만 띄우는 컨트롤러다. */
UCLASS()
class HEAVENHYPERVOICE_API AUECharacterSelectionPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// 화면에 표시할 위젯은 BP_CharacterSelectionPlayerController의 기본값에서 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Selection")
	TSubclassOf<UUECharacterSelectionWidget> SelectionWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Selection")
	int32 WidgetZOrder = 100;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUECharacterSelectionWidget> SelectionWidget = nullptr;
};
