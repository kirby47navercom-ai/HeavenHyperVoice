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
    // DA의 퍼지 기준/규칙을 서버와 같은 계산기에 전달해요. 이 실행 경로는 Yang2 전용이에요.
    auto& Fuzzy=Result.environment.fuzzyWeather;const auto& Source=Asset->FuzzyWeather;
    Fuzzy.humidStartPct=Source.HumidStartPct;Fuzzy.humidFullPct=Source.HumidFullPct;
    Fuzzy.fogHumidStartPct=Source.FogHumidStartPct;Fuzzy.fogHumidFullPct=Source.FogHumidFullPct;
    Fuzzy.lowPressureFullDeficitHpa=Source.LowPressureFullDeficitHpa;Fuzzy.cloudFullCover=Source.CloudFullCover;
    Fuzzy.fullSnowTemperatureC=Source.FullSnowTemperatureC;Fuzzy.fullRainTemperatureC=Source.FullRainTemperatureC;
    Fuzzy.fogCoolingFullC=Source.FogCoolingFullC;
    Fuzzy.fogCalmStartMps=Source.FogCalmStartMps;Fuzzy.fogCalmEndMps=Source.FogCalmEndMps;
    Fuzzy.soilDryStart=Source.SoilDryStart;Fuzzy.soilDryFull=Source.SoilDryFull;
    const bool RuleCountValid=Source.RainRuleOutputs.Num()==8 && Source.FogRuleOutputs.Num()==8 && Source.DustRuleOutputs.Num()==8;
    if(RuleCountValid) for(int32 Index=0;Index<8;++Index) {
        Fuzzy.rainRuleOutputs[Index]=Source.RainRuleOutputs[Index];
        Fuzzy.fogRuleOutputs[Index]=Source.FogRuleOutputs[Index];
        Fuzzy.dustRuleOutputs[Index]=Source.DustRuleOutputs[Index];
    }
    if(!RuleCountValid || !heaven::instance::validFuzzyWeatherProfile(Fuzzy)) {
        UE_LOG(LogTemp,Warning,TEXT("Invalid FuzzyWeather in %s; using default fuzzy rules."),*Asset->GetPathName());
        Fuzzy=heaven::instance::FuzzyWeatherProfile{};
    }
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
    Result.environment.surfaceFilmCapacityKgM2=Asset->SurfaceFilmCapacityKgM2;
    Result.environment.moistureExchangeSeconds=Asset->MoistureExchangeSeconds;
    Result.environment.solarPeakWm2=Asset->SolarPeakWm2;
    Result.environment.surfaceAlbedo=Asset->SurfaceAlbedo;
    Result.environment.surfaceEmissivity=Asset->SurfaceEmissivity;
    Result.environment.clearSkyCoolingWm2=Asset->ClearSkyCoolingWm2;
    Result.environment.airHeatTransferWm2K=Asset->AirHeatTransferWm2K;
    Result.environment.springNeapPeriodDays=Asset->SpringNeapPeriodDays;
    Result.environment.neapTideFraction=Asset->NeapTideFraction;
    Result.environment.springNeapPhaseDegrees=Asset->SpringNeapPhaseDegrees;
    Result.environment.shoreHeightM=Asset->ShoreHeightM;
    Result.environment.wetMovementMultiplier=Asset->WetMovementMultiplier;
    Result.environment.iceMovementMultiplier=Asset->IceMovementMultiplier;
    Result.environment.sandVisibilityMultiplier=Asset->SandVisibilityMultiplier;
    Result.environment.iceTractionMultiplier=Asset->IceTractionMultiplier;
    Result.environment.swimSpeedCmPerSecond=Asset->SwimSpeedCmPerSecond;
    Result.environment.wildRespawnSeconds=Asset->WildRespawnSeconds;
    Result.environment.waterRegions.clear();
    for(const auto& Region:Asset->WaterRegions) if(hhv::movement::valid(Region.ToCore())) Result.environment.waterRegions.push_back(Region.ToCore());
    Result.spawnRules.clear();
    TSet<int32> UsedDex;
    for(const auto& Row:Asset->SpawnRules) {
        if(Row.PokemonDex<1 || Row.PokemonDex>65535 || UsedDex.Contains(Row.PokemonDex)) continue;
        bool Valid=true;
        for(double V:{Row.BaseWeight,Row.RainMultiplier,Row.SnowMultiplier,Row.NightMultiplier})
            Valid=Valid && FMath::IsFinite(V) && V>=0 && V<=100;
        if(!Valid) continue;
        UsedDex.Add(Row.PokemonDex);
        Result.spawnRules.push_back({static_cast<uint16>(Row.PokemonDex),Row.BaseWeight,Row.RainMultiplier,Row.SnowMultiplier,Row.NightMultiplier});
    }
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
    Result.SurfaceHeatFluxWm2=Source.surfaceHeatFluxWm2;
    Result.ImportedWaterKgM2=Source.importedWaterKgM2;
    Result.ExportedWaterKgM2=Source.exportedWaterKgM2;
    Result.TideEnvelopeM=Source.tideEnvelopeM;
    Result.ShoreWaterDepthM=Source.shoreWaterDepthM;
    Result.MovementMultiplier=Source.movementMultiplier;
    Result.VisibilityMultiplier=Source.visibilityMultiplier;
    Result.FuzzyWeatherEnabled=Source.fuzzyWeatherEnabled;
    Result.SnowFraction=Source.snowFraction;
    Result.FogDensity=Source.fogDensity;
    return Result;
}
