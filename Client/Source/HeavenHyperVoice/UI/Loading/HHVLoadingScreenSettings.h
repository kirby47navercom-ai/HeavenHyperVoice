#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HHVLoadingScreenSettings.generated.h"

class UUserWidget;

/** 로딩 동작 설정. 화면 에셋은 공통 에셋 연결표에서 지정한다. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "로딩 화면"))
class HEAVENHYPERVOICE_API UHHVLoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** 아주 짧은 이동에서도 화면이 번쩍이지 않도록 유지할 최소 시간이다. */
	UPROPERTY(Config, EditAnywhere, Category = "Behavior", meta = (ClampMin = "0.0", Units = "s"))
	float MinimumDisplaySeconds = 0.35f;

	/** 레벨, 머테리얼, 메시, 텍스처 스트리밍이 끝날 때까지 화면을 닫지 않는다. */
	UPROPERTY(Config, EditAnywhere, Category = "Behavior")
	bool bWaitForAssetStreaming = true;
};
