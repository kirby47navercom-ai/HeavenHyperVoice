// YANG2_CLIENT_AUTHORITY_ONLY
// 데이터 에셋 → 서버 계산 입력, 계산 결과 → 공통 연출 상태를 변환하는 로컬 전용 어댑터.
#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "Local environment adapter must not compile on main."
#endif
#include "UEYang2Environment.h"
#include "../Environment/UEEnvironmentProfile.h"
#include "../Environment/UEEnvironmentScene.h"
#include "EngineUtils.h"
void ApplyYang2EnvironmentProfile(UWorld* World,const UUEEnvironmentProfile* Override,
    heaven::instance::InstanceWeatherProfile& Result) {
    // 인스턴스 종류별 명시 참조가 우선이다. 없으면 레벨에 배치한 Scene의 Profile을 읽는다.
    const UUEEnvironmentProfile* Asset=Override;
    if(!Asset && World) for(TActorIterator<AUEEnvironmentScene> It(World);It;++It) {
        if(It->Profile) { Asset=It->Profile; break; }
    }
    if(!Asset) return; // 기존 Yang2WeatherProfiles의 여섯 값도 계속 사용할 수 있다.
    if(FMath::IsFinite(Asset->MeanTemperatureC)) Result.meanTemperatureC=FMath::Clamp(Asset->MeanTemperatureC,-60.0,60.0);
    if(FMath::IsFinite(Asset->InitialRelativeHumidityPct)) Result.initialRelativeHumidityPct=FMath::Clamp(Asset->InitialRelativeHumidityPct,0.0,100.0);
    if(FMath::IsFinite(Asset->MeanPressureHpa)) Result.meanPressureHpa=FMath::Clamp(Asset->MeanPressureHpa,800.0,1100.0);
    if(FMath::IsFinite(Asset->InitialSurfaceWaterKgM2)) Result.initialSurfaceWaterKgM2=FMath::Clamp(Asset->InitialSurfaceWaterKgM2,0.0,1000.0);
    if(FMath::IsFinite(Asset->InitialSoilWaterKgM2)) Result.initialSoilWaterKgM2=FMath::Clamp(Asset->InitialSoilWaterKgM2,0.0,10000.0);
    if(FMath::IsFinite(Asset->GameSecondsPerRealSecond)) Result.gameSecondsPerRealSecond=FMath::Clamp(Asset->GameSecondsPerRealSecond,0.0,3600.0);
    Result.environment.daySeconds=Asset->DaySeconds;
    Result.environment.yearDays=Asset->YearDays;
    Result.environment.startHour=Asset->StartHour;
    Result.environment.startYearFraction=Asset->StartYearFraction;
    Result.environment.latitudeDegrees=Asset->LatitudeDegrees;
    Result.environment.altitudeM=Asset->AltitudeM;
    Result.environment.seasonalAmplitudeC=Asset->SeasonalAmplitudeC;
    Result.environment.dailyAmplitudeC=Asset->DailyAmplitudeC;
    Result.environment.thermalResponseSeconds=Asset->ThermalResponseSeconds;
    Result.environment.soilCapacityKgM2=Asset->SoilCapacityKgM2;
    Result.environment.fieldCapacityKgM2=Asset->FieldCapacityKgM2;
    Result.environment.infiltrationKgM2PerSecond=Asset->InfiltrationKgM2PerSecond;
    Result.environment.drainageSeconds=Asset->DrainageSeconds;
    Result.environment.baseWindMps=Asset->BaseWindMps;
    Result.environment.gustAmplitudeMps=Asset->GustAmplitudeMps;
    Result.environment.sandAvailability=Asset->SandAvailability;
    Result.environment.dustStartWindMps=Asset->DustStartWindMps;
    Result.environment.dustFullWindMps=Asset->DustFullWindMps;
    Result.environment.dustResponseSeconds=Asset->DustResponseSeconds;
    Result.environment.tideAmplitudeM=Asset->TideAmplitudeM;
    Result.environment.tidePeriodHours=Asset->TidePeriodHours;
    Result.environment.tidePhaseDegrees=Asset->TidePhaseDegrees;
    Result.environment.waveMaxHeightM=Asset->WaveMaxHeightM;
    Result.environment.waveResponseSeconds=Asset->WaveResponseSeconds;
    heaven::instance::normalizeEnvironment(Result.environment);
    Result.initialSoilWaterKgM2=FMath::Min(Result.initialSoilWaterKgM2,Result.environment.soilCapacityKgM2);
}
FUEEnvironmentState MakeYang2EnvironmentState(const heaven::instance::EnvironmentState& Source) {
    FUEEnvironmentState Result;Result.Enabled=true;
    Result.DayFraction=Source.dayFraction;
    Result.YearFraction=Source.yearFraction;
    Result.SunElevationDegrees=Source.sunElevationDegrees;
    Result.SunAzimuthDegrees=Source.sunAzimuthDegrees;
    Result.TideLevelM=Source.tideLevelM;
    Result.WaveHeightM=Source.waveHeightM;
    Result.SandstormIntensity=Source.sandstormIntensity;
    Result.GroundTemperatureC=Source.groundTemperatureC;
    Result.SurfaceWaterMm=Source.surfaceWaterMm;
    Result.IceMm=Source.iceMm;
    Result.SoilMoisture=Source.soilMoisture;
    Result.TimeScale=Source.timeScale;
    return Result;
}
