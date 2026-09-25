#include "UEInstanceWeatherPresentationComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

namespace
{
	float Normalize(float Value, float FullValue)
	{
		return FMath::Clamp(Value / FMath::Max(FullValue, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	}

	float Move(float Current, float Target, float Alpha)
	{
		return FMath::Lerp(Current, Target, Alpha);
	}
}

UUEInstanceWeatherPresentationComponent::UUEInstanceWeatherPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UUEInstanceWeatherPresentationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bFindWeatherSourceAutomatically)
	{
		UUEFieldServerBridgeComponent* Source = GetOwner()
			? GetOwner()->FindComponentByClass<UUEFieldServerBridgeComponent>()
			: nullptr;
		if (!Source && GetWorld())
		{
			Source = UUEFieldServerBridgeComponent::Find(GetWorld()->GetFirstPlayerController());
		}
		SetWeatherSource(Source);
		SetComponentTickEnabled(Source == nullptr);
	}
}

void UUEInstanceWeatherPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bFindWeatherSourceAutomatically = false;
	SetWeatherSource(nullptr);
	SetComponentTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void UUEInstanceWeatherPresentationComponent::SetWeatherSource(
	UUEFieldServerBridgeComponent* NewSource)
{
	if (WeatherSource == NewSource)
	{
		return;
	}

	if (WeatherSource)
	{
		WeatherSource->OnInstanceWeatherChanged.RemoveDynamic(
			this, &UUEInstanceWeatherPresentationComponent::HandleWeatherChanged);
	}

	WeatherSource = NewSource;
	if (!WeatherSource)
	{
		return;
	}

	WeatherSource->OnInstanceWeatherChanged.AddUniqueDynamic(
		this, &UUEInstanceWeatherPresentationComponent::HandleWeatherChanged);
	if (WeatherSource->HasInstanceWeatherState())
	{
		HandleWeatherChanged(WeatherSource->GetInstanceWeatherState());
	}
}

FUEInstanceWeatherPresentationState
UUEInstanceWeatherPresentationComponent::MakeTargetState(
	const FUEInstanceWeatherState& Weather) const
{
	FUEInstanceWeatherPresentationState Result;
	Result.TemperatureC = Weather.TemperatureC;
	Result.RelativeHumidityPct = Weather.RelativeHumidityPct;
	Result.PressureHpa = Weather.PressureHpa;
	Result.CloudAmount = FMath::Clamp(Weather.CloudCover, 0.0f, 1.0f);

	const float Precipitation = Normalize(
		Weather.PrecipitationMmPerHour, HeavyPrecipitationMmPerHour);
	const float TemperatureRange = FMath::Max(
		FullRainTemperatureC - FullSnowTemperatureC, KINDA_SMALL_NUMBER);
	const float RainFraction = FMath::Clamp(
		(Weather.TemperatureC - FullSnowTemperatureC) / TemperatureRange, 0.0f, 1.0f);
	Result.RainIntensity = Precipitation * RainFraction;
	Result.SnowIntensity = Precipitation * (1.0f - RainFraction);

	const float FogRange = FMath::Max(
		FullFogHumidityPct - FogStartHumidityPct, KINDA_SMALL_NUMBER);
	const float HumidityFog = FMath::Clamp(
		(Weather.RelativeHumidityPct - FogStartHumidityPct) / FogRange, 0.0f, 1.0f);
	Result.FogDensity = HumidityFog * FMath::Lerp(0.35f, 1.0f, Result.CloudAmount);

	Result.WindIntensity = Normalize(Weather.WindSpeedMps, StrongWindMps);
	Result.WindDirectionDegrees = FMath::Fmod(
		FMath::Fmod(Weather.WindDirectionDegrees, 360.0f) + 360.0f, 360.0f);
	Result.GroundWetness = FMath::Clamp(Weather.GroundWetness, 0.0f, 1.0f);
	Result.SnowCoverage = Normalize(Weather.SnowDepthM, FullSnowCoverageDepthM);
	Result.bRaining = Result.RainIntensity > 0.01f;
	Result.bSnowing = Result.SnowIntensity > 0.01f;
	Result.bFoggy = Result.FogDensity > 0.01f;
	return Result;
}

void UUEInstanceWeatherPresentationComponent::HandleWeatherChanged(
	const FUEInstanceWeatherState& Weather)
{
	TargetState = MakeTargetState(Weather);
	if (!bHasWeather)
	{
		CurrentState = TargetState;
		bHasWeather = true;
		BroadcastCurrentState();
	}
	SetComponentTickEnabled(true);
}

void UUEInstanceWeatherPresentationComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!WeatherSource && bFindWeatherSourceAutomatically)
	{
		SourceSearchSeconds += FMath::Max(0.0f, DeltaTime);
		if (SourceSearchSeconds >= 1.0f)
		{
			SourceSearchSeconds = 0.0f;
			SetWeatherSource(UUEFieldServerBridgeComponent::Find(
				GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr));
		}
		if (!WeatherSource)
		{
			return;
		}
	}
	if (!bHasWeather)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const float Alpha = 1.0f - FMath::Exp(
		-FMath::Max(0.1f, TransitionSpeed) * FMath::Max(0.0f, DeltaTime));
	CurrentState.TemperatureC = Move(CurrentState.TemperatureC, TargetState.TemperatureC, Alpha);
	CurrentState.RelativeHumidityPct = Move(
		CurrentState.RelativeHumidityPct, TargetState.RelativeHumidityPct, Alpha);
	CurrentState.PressureHpa = Move(CurrentState.PressureHpa, TargetState.PressureHpa, Alpha);
	CurrentState.CloudAmount = Move(CurrentState.CloudAmount, TargetState.CloudAmount, Alpha);
	CurrentState.RainIntensity = Move(CurrentState.RainIntensity, TargetState.RainIntensity, Alpha);
	CurrentState.SnowIntensity = Move(CurrentState.SnowIntensity, TargetState.SnowIntensity, Alpha);
	CurrentState.FogDensity = Move(CurrentState.FogDensity, TargetState.FogDensity, Alpha);
	CurrentState.WindIntensity = Move(CurrentState.WindIntensity, TargetState.WindIntensity, Alpha);
	CurrentState.GroundWetness = Move(CurrentState.GroundWetness, TargetState.GroundWetness, Alpha);
	CurrentState.SnowCoverage = Move(CurrentState.SnowCoverage, TargetState.SnowCoverage, Alpha);

	const float DirectionDelta = FMath::FindDeltaAngleDegrees(
		CurrentState.WindDirectionDegrees, TargetState.WindDirectionDegrees);
	CurrentState.WindDirectionDegrees = FMath::Fmod(
		CurrentState.WindDirectionDegrees + DirectionDelta * Alpha + 360.0f, 360.0f);
	CurrentState.bRaining = CurrentState.RainIntensity > 0.01f;
	CurrentState.bSnowing = CurrentState.SnowIntensity > 0.01f;
	CurrentState.bFoggy = CurrentState.FogDensity > 0.01f;

	BroadcastCurrentState();

	const bool bSettled =
		FMath::IsNearlyEqual(CurrentState.TemperatureC, TargetState.TemperatureC, 0.01f) &&
		FMath::IsNearlyEqual(CurrentState.RelativeHumidityPct, TargetState.RelativeHumidityPct, 0.01f) &&
		FMath::IsNearlyEqual(CurrentState.PressureHpa, TargetState.PressureHpa, 0.01f) &&
		FMath::IsNearlyEqual(CurrentState.CloudAmount, TargetState.CloudAmount, 0.001f) &&
		FMath::IsNearlyEqual(CurrentState.RainIntensity, TargetState.RainIntensity, 0.001f) &&
		FMath::IsNearlyEqual(CurrentState.SnowIntensity, TargetState.SnowIntensity, 0.001f) &&
		FMath::IsNearlyEqual(CurrentState.FogDensity, TargetState.FogDensity, 0.001f) &&
		FMath::IsNearlyEqual(CurrentState.WindIntensity, TargetState.WindIntensity, 0.001f) &&
		FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentState.WindDirectionDegrees,
			TargetState.WindDirectionDegrees)) < 0.1f &&
		FMath::IsNearlyEqual(CurrentState.GroundWetness, TargetState.GroundWetness, 0.001f) &&
		FMath::IsNearlyEqual(CurrentState.SnowCoverage, TargetState.SnowCoverage, 0.001f);
	if (bSettled)
	{
		CurrentState = TargetState;
		BroadcastCurrentState();
		SetComponentTickEnabled(false);
	}
}

void UUEInstanceWeatherPresentationComponent::BroadcastCurrentState()
{
	OnWeatherPresentationUpdated.Broadcast(CurrentState);
}
