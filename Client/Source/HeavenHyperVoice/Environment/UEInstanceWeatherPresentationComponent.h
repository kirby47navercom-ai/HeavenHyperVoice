#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Server/UEFieldServerBridgeComponent.h"
#include "UEInstanceWeatherPresentationComponent.generated.h"

/**
 * 서버의 물리 단위를 블루프린트 연출이 바로 사용할 수 있는 0~1 값으로 바꾼 결과.
 * 이 구조체는 비와 눈을 직접 생성하지 않는다. Niagara, 머티리얼, 사운드는 이 값을
 * 받는 블루프린트에서 배치한다.
 */
USTRUCT(BlueprintType)
struct FUEInstanceWeatherPresentationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Raw")
	float TemperatureC = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Raw")
	float RelativeHumidityPct = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Raw")
	float PressureHpa = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Presentation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CloudAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Presentation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RainIntensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Presentation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SnowIntensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Presentation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FogDensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Presentation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WindIntensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Presentation", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float WindDirectionDegrees = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Presentation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GroundWetness = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Presentation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SnowCoverage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Gameplay")
	bool bRaining = false;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Gameplay")
	bool bSnowing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Instance Weather|Gameplay")
	bool bFoggy = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUEOnWeatherPresentationUpdated,
	const FUEInstanceWeatherPresentationState&, Weather);

/**
 * InstanceServer 또는 Yang2 로컬 계산에서 온 같은 날씨 이벤트를 받아 연출값으로 바꾼다.
 * 서버 권위 여부를 판단하지 않으므로 main과 Yang2가 같은 블루프린트를 사용할 수 있다.
 */
UCLASS(ClassGroup = (Environment), meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEInstanceWeatherPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEInstanceWeatherPresentationComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 같은 액터에서 찾지 못했을 때 블루프린트가 날씨 브릿지를 직접 지정한다. */
	UFUNCTION(BlueprintCallable, Category = "Instance Weather")
	void SetWeatherSource(UUEFieldServerBridgeComponent* NewSource);

	UFUNCTION(BlueprintPure, Category = "Instance Weather")
	FUEInstanceWeatherPresentationState GetPresentationState() const { return CurrentState; }

	/** 값이 갱신되는 동안 호출한다. 블루프린트에서 Niagara와 머티리얼에 연결한다. */
	UPROPERTY(BlueprintAssignable, Category = "Instance Weather")
	FUEOnWeatherPresentationUpdated OnWeatherPresentationUpdated;

	/** 같은 액터를 먼저 보고, 없으면 첫 번째 로컬 플레이어의 브릿지를 찾는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Weather|Binding")
	bool bFindWeatherSourceAutomatically = true;

	/** 서버에서 새 값이 왔을 때 화면이 그 값에 도달하는 속도. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Weather|Tuning", meta = (ClampMin = "0.1"))
	float TransitionSpeed = 2.5f;

	/** 이 강수량 이상을 연출 강도 1로 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Weather|Tuning", meta = (ClampMin = "0.1", Units = "MillimetersPerHour"))
	float HeavyPrecipitationMmPerHour = 12.0f;

	/** 이 풍속 이상을 연출 강도 1로 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Weather|Tuning", meta = (ClampMin = "0.1", Units = "MetersPerSecond"))
	float StrongWindMps = 15.0f;

	/** 이 습도부터 안개 연출이 시작된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Weather|Tuning", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float FogStartHumidityPct = 85.0f;

	/** 이 습도에서 안개 연출이 최대가 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Weather|Tuning", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float FullFogHumidityPct = 100.0f;

	/** 이 온도 이하는 강수를 전부 눈으로 표현한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Weather|Tuning", meta = (Units = "Celsius"))
	float FullSnowTemperatureC = 0.0f;

	/** 이 온도 이상은 강수를 전부 비로 표현한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Weather|Tuning", meta = (Units = "Celsius"))
	float FullRainTemperatureC = 2.0f;

	/** 이 적설 깊이 이상을 지면 눈 덮임 1로 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Weather|Tuning", meta = (ClampMin = "0.001", Units = "Meters"))
	float FullSnowCoverageDepthM = 0.10f;

private:
	UFUNCTION()
	void HandleWeatherChanged(const FUEInstanceWeatherState& Weather);

	FUEInstanceWeatherPresentationState MakeTargetState(const FUEInstanceWeatherState& Weather) const;
	void BroadcastCurrentState();

	UPROPERTY(Transient)
	TObjectPtr<UUEFieldServerBridgeComponent> WeatherSource;

	FUEInstanceWeatherPresentationState CurrentState;
	FUEInstanceWeatherPresentationState TargetState;
	bool bHasWeather = false;
	float SourceSearchSeconds = 0.0f;
};
