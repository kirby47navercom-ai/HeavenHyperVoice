#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "UEVoiceWave.generated.h"

class SUEVoiceWave;

/**
 * 막대 몇 개가 차례로 커졌다 작아지는 음성 파형.
 *
 * UMG 애니메이션이 아니라 그릴 때마다 시간으로 높이를 계산한다. 그래서 게임 스레드가 멈춘
 * 로딩 화면(무비 플레이어 로딩 스레드)에서도 움직인다.
 */
UCLASS()
class HEAVENHYPERVOICE_API UUEVoiceWave : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice Wave", meta = (ClampMin = "1", ClampMax = "16"))
	int32 BarCount = 5;

	// 가장 클 때 막대 하나의 크기.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice Wave")
	FVector2D BarSize = FVector2D(7.0, 34.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice Wave", meta = (ClampMin = "0.0"))
	float BarGap = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice Wave")
	FLinearColor BarColor = FLinearColor::White;

	// 막대 하나가 커졌다 작아지는 한 번의 시간.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice Wave", meta = (ClampMin = "0.1", Units = "s"))
	float Period = 1.1f;

	// 가장 작을 때 높이 비율. 0 이면 막대가 사라지는 순간이 생긴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice Wave", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinScale = 0.3f;

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SUEVoiceWave> MyWave;
};
