#include "UEVoiceWave.h"

#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

class SUEVoiceWave : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SUEVoiceWave) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
	}

	void SetStyle(int32 InBarCount, FVector2D InBarSize, float InBarGap, FLinearColor InColor, float InPeriod, float InMinScale)
	{
		BarCount = FMath::Max(1, InBarCount);
		BarSize = InBarSize;
		BarGap = InBarGap;
		Color = InColor;
		Period = FMath::Max(0.1f, InPeriod);
		MinScale = FMath::Clamp(InMinScale, 0.0f, 1.0f);
		Invalidate(EInvalidateWidgetReason::Layout);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(BarCount * BarSize.X + (BarCount - 1) * BarGap, BarSize.Y);
	}

	// 매 프레임 다시 그려야 움직인다.
	virtual bool ComputeVolatility() const override
	{
		return true;
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		// 로딩 스레드에서도 쓰이므로 게임 시간 대신 실제 시간을 쓴다.
		const double Time = FPlatformTime::Seconds();
		const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		const FLinearColor Tint = Color * InWidgetStyle.GetColorAndOpacityTint();
		const float Height = AllottedGeometry.GetLocalSize().Y;

		for (int32 Index = 0; Index < BarCount; ++Index)
		{
			// 막대마다 위상을 밀어 왼쪽에서 오른쪽으로 흐르는 파형을 만든다.
			const double Phase = 2.0 * UE_DOUBLE_PI * (Time / Period) - Index * 0.9;
			const float Scale = MinScale + (1.0f - MinScale) * static_cast<float>(0.5 + 0.5 * FMath::Sin(Phase));
			const float BarHeight = BarSize.Y * Scale;
			const FVector2f Offset(Index * (BarSize.X + BarGap), (Height - BarHeight) * 0.5f);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2f(BarSize.X, BarHeight), FSlateLayoutTransform(Offset)),
				Brush,
				ESlateDrawEffect::None,
				Tint);
		}
		return LayerId;
	}

private:
	int32 BarCount = 5;
	FVector2D BarSize = FVector2D(7.0, 34.0);
	float BarGap = 5.0f;
	FLinearColor Color = FLinearColor::White;
	float Period = 1.1f;
	float MinScale = 0.3f;
};

TSharedRef<SWidget> UUEVoiceWave::RebuildWidget()
{
	MyWave = SNew(SUEVoiceWave);
	return MyWave.ToSharedRef();
}

void UUEVoiceWave::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (MyWave)
	{
		MyWave->SetStyle(BarCount, BarSize, BarGap, BarColor, Period, MinScale);
	}
}

void UUEVoiceWave::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyWave.Reset();
}
