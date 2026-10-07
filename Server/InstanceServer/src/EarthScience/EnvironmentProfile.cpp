#include "EnvironmentProfile.h"
#include <algorithm>
#include <cmath>
namespace heaven::instance {
void normalizeEnvironment(EnvironmentProfile& p) {
    normalizeFuzzyWeatherProfile(p.fuzzyWeather);
    const EnvironmentProfile defaults;
    p.iceTractionMultiplier=std::isfinite(p.iceTractionMultiplier) ? std::clamp(p.iceTractionMultiplier,.02,1.) : defaults.iceTractionMultiplier;
    p.swimSpeedCmPerSecond=std::isfinite(p.swimSpeedCmPerSecond) ? std::clamp(p.swimSpeedCmPerSecond,10.,1000.) : defaults.swimSpeedCmPerSecond;
    p.wildRespawnSeconds=std::isfinite(p.wildRespawnSeconds) ? std::clamp(p.wildRespawnSeconds,1.,86400.) : defaults.wildRespawnSeconds;
    p.waterRegions.erase(std::remove_if(p.waterRegions.begin(),p.waterRegions.end(),[](const auto& r){return !hhv::movement::valid(r);}),p.waterRegions.end());
    if(p.waterRegions.size()>64) p.waterRegions.resize(64);
    p.daySeconds=std::isfinite(p.daySeconds) ? std::clamp(p.daySeconds,60.0,864000.0) : defaults.daySeconds;
    p.yearDays=std::isfinite(p.yearDays) ? std::clamp(p.yearDays,1.0,1000.0) : defaults.yearDays;
    p.startHour=std::isfinite(p.startHour) ? std::clamp(p.startHour,0.0,24.0) : defaults.startHour;
    p.startYearFraction=std::isfinite(p.startYearFraction) ? std::clamp(p.startYearFraction,0.0,1.0) : defaults.startYearFraction;
    p.latitudeDegrees=std::isfinite(p.latitudeDegrees) ? std::clamp(p.latitudeDegrees,-85.0,85.0) : defaults.latitudeDegrees;
    p.altitudeM=std::isfinite(p.altitudeM) ? std::clamp(p.altitudeM,-500.0,9000.0) : defaults.altitudeM;
    p.seasonalAmplitudeC=std::isfinite(p.seasonalAmplitudeC) ? std::clamp(p.seasonalAmplitudeC,0.0,40.0) : defaults.seasonalAmplitudeC;
    p.dailyAmplitudeC=std::isfinite(p.dailyAmplitudeC) ? std::clamp(p.dailyAmplitudeC,0.0,40.0) : defaults.dailyAmplitudeC;
    p.thermalResponseSeconds=std::isfinite(p.thermalResponseSeconds) ? std::clamp(p.thermalResponseSeconds,1.0,864000.0) : defaults.thermalResponseSeconds;
    p.soilCapacityKgM2=std::isfinite(p.soilCapacityKgM2) ? std::clamp(p.soilCapacityKgM2,0.001,10000.0) : defaults.soilCapacityKgM2;
    p.fieldCapacityKgM2=std::isfinite(p.fieldCapacityKgM2) ? std::clamp(p.fieldCapacityKgM2,0.0,10000.0) : defaults.fieldCapacityKgM2;
    p.infiltrationKgM2PerSecond=std::isfinite(p.infiltrationKgM2PerSecond) ? std::clamp(p.infiltrationKgM2PerSecond,0.0,10.0) : defaults.infiltrationKgM2PerSecond;
    p.drainageSeconds=std::isfinite(p.drainageSeconds) ? std::clamp(p.drainageSeconds,1.0,86400000.0) : defaults.drainageSeconds;
    p.baseWindMps=std::isfinite(p.baseWindMps) ? std::clamp(p.baseWindMps,0.0,100.0) : defaults.baseWindMps;
    p.gustAmplitudeMps=std::isfinite(p.gustAmplitudeMps) ? std::clamp(p.gustAmplitudeMps,0.0,100.0) : defaults.gustAmplitudeMps;
    p.sandAvailability=std::isfinite(p.sandAvailability) ? std::clamp(p.sandAvailability,0.0,1.0) : defaults.sandAvailability;
    p.dustStartWindMps=std::isfinite(p.dustStartWindMps) ? std::clamp(p.dustStartWindMps,0.0,100.0) : defaults.dustStartWindMps;
    p.dustFullWindMps=std::isfinite(p.dustFullWindMps) ? std::clamp(p.dustFullWindMps,0.1,200.0) : defaults.dustFullWindMps;
    p.dustResponseSeconds=std::isfinite(p.dustResponseSeconds) ? std::clamp(p.dustResponseSeconds,1.0,86400.0) : defaults.dustResponseSeconds;
    p.tideAmplitudeM=std::isfinite(p.tideAmplitudeM) ? std::clamp(p.tideAmplitudeM,0.0,20.0) : defaults.tideAmplitudeM;
    p.tidePeriodHours=std::isfinite(p.tidePeriodHours) ? std::clamp(p.tidePeriodHours,0.01,1000.0) : defaults.tidePeriodHours;
    p.tidePhaseDegrees=std::isfinite(p.tidePhaseDegrees) ? std::clamp(p.tidePhaseDegrees,0.0,360.0) : defaults.tidePhaseDegrees;
    p.waveMaxHeightM=std::isfinite(p.waveMaxHeightM) ? std::clamp(p.waveMaxHeightM,0.0,20.0) : defaults.waveMaxHeightM;
    p.waveResponseSeconds=std::isfinite(p.waveResponseSeconds) ? std::clamp(p.waveResponseSeconds,1.0,86400.0) : defaults.waveResponseSeconds;
    p.surfaceFilmCapacityKgM2=std::isfinite(p.surfaceFilmCapacityKgM2) ? std::clamp(p.surfaceFilmCapacityKgM2,0.001,10.0) : defaults.surfaceFilmCapacityKgM2;
    p.moistureExchangeSeconds=std::isfinite(p.moistureExchangeSeconds) ? std::clamp(p.moistureExchangeSeconds,60.0,8640000.0) : defaults.moistureExchangeSeconds;
    p.solarPeakWm2=std::isfinite(p.solarPeakWm2) ? std::clamp(p.solarPeakWm2,0.0,1400.0) : defaults.solarPeakWm2;
    p.surfaceAlbedo=std::isfinite(p.surfaceAlbedo) ? std::clamp(p.surfaceAlbedo,0.0,1.0) : defaults.surfaceAlbedo;
    p.surfaceEmissivity=std::isfinite(p.surfaceEmissivity) ? std::clamp(p.surfaceEmissivity,0.01,1.0) : defaults.surfaceEmissivity;
    p.clearSkyCoolingWm2=std::isfinite(p.clearSkyCoolingWm2) ? std::clamp(p.clearSkyCoolingWm2,0.0,200.0) : defaults.clearSkyCoolingWm2;
    p.airHeatTransferWm2K=std::isfinite(p.airHeatTransferWm2K) ? std::clamp(p.airHeatTransferWm2K,1.0,200.0) : defaults.airHeatTransferWm2K;
    p.springNeapPeriodDays=std::isfinite(p.springNeapPeriodDays) ? std::clamp(p.springNeapPeriodDays,0.1,1000.0) : defaults.springNeapPeriodDays;
    p.neapTideFraction=std::isfinite(p.neapTideFraction) ? std::clamp(p.neapTideFraction,0.0,1.0) : defaults.neapTideFraction;
    p.springNeapPhaseDegrees=std::isfinite(p.springNeapPhaseDegrees) ? std::clamp(p.springNeapPhaseDegrees,0.0,360.0) : defaults.springNeapPhaseDegrees;
    p.shoreHeightM=std::isfinite(p.shoreHeightM) ? std::clamp(p.shoreHeightM,-100.0,100.0) : defaults.shoreHeightM;
    p.wetMovementMultiplier=std::isfinite(p.wetMovementMultiplier) ? std::clamp(p.wetMovementMultiplier,0.1,1.0) : defaults.wetMovementMultiplier;
    p.iceMovementMultiplier=std::isfinite(p.iceMovementMultiplier) ? std::clamp(p.iceMovementMultiplier,0.1,1.0) : defaults.iceMovementMultiplier;
    p.sandVisibilityMultiplier=std::isfinite(p.sandVisibilityMultiplier) ? std::clamp(p.sandVisibilityMultiplier,0.1,1.0) : defaults.sandVisibilityMultiplier;
    p.fieldCapacityKgM2=std::min(p.fieldCapacityKgM2,p.soilCapacityKgM2);
    p.dustFullWindMps=std::max(p.dustFullWindMps,p.dustStartWindMps+.1);
}
}
