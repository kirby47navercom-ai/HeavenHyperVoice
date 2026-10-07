#include "../InstanceWeather.h"
#include <array>
#include <cmath>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>

namespace heaven::instance {
namespace {
// 설정 변경 후 과거 상태를 다른 물리 규칙에 잘못 이어 붙이지 않아요.
std::uint64_t profileKey(const InstanceWeatherProfile& profile) {
    std::ostringstream out;out<<std::setprecision(17);const auto& p=profile.environment;
    for(double v:{
        p.iceTractionMultiplier,p.swimSpeedCmPerSecond,p.wildRespawnSeconds,p.daySeconds,
        p.yearDays,p.startHour,p.startYearFraction,p.latitudeDegrees,
        p.altitudeM,p.seasonalAmplitudeC,p.dailyAmplitudeC,p.thermalResponseSeconds,
        p.soilCapacityKgM2,p.fieldCapacityKgM2,p.infiltrationKgM2PerSecond,p.drainageSeconds,
        p.baseWindMps,p.gustAmplitudeMps,p.sandAvailability,p.dustStartWindMps,
        p.dustFullWindMps,p.dustResponseSeconds,p.tideAmplitudeM,p.tidePeriodHours,
        p.tidePhaseDegrees,p.waveMaxHeightM,p.waveResponseSeconds,p.surfaceFilmCapacityKgM2,
        p.moistureExchangeSeconds,p.solarPeakWm2,p.surfaceAlbedo,p.surfaceEmissivity,
        p.clearSkyCoolingWm2,p.airHeatTransferWm2K,p.springNeapPeriodDays,p.neapTideFraction,
        p.springNeapPhaseDegrees,p.shoreHeightM,p.wetMovementMultiplier,p.iceMovementMultiplier,
        p.sandVisibilityMultiplier,profile.meanTemperatureC,profile.initialRelativeHumidityPct,profile.meanPressureHpa,
        profile.initialSurfaceWaterKgM2,profile.initialSoilWaterKgM2,profile.gameSecondsPerRealSecond
    }) out<<v<<' ';
    // 기본 규칙은 기존 CLIMATE1의 키를 유지해요. 새 규칙으로 계속 진행할 수 있어요.
    // 사용자 조절값이 다르면 다른 기후 설정이므로 기존의 복구 거절 절차를 유지해요.
    const auto& f=p.fuzzyWeather;const FuzzyWeatherProfile defaults;
    std::ostringstream fuzzy,original;fuzzy<<std::setprecision(17);original<<std::setprecision(17);
    const auto append=[](std::ostream& stream,const FuzzyWeatherProfile& value) {
        for(double v:{value.humidStartPct,value.humidFullPct,value.fogHumidStartPct,value.fogHumidFullPct,
            value.lowPressureFullDeficitHpa,value.cloudFullCover,value.fullSnowTemperatureC,value.fullRainTemperatureC,
            value.fogCoolingFullC,value.fogCalmStartMps,value.fogCalmEndMps,value.soilDryStart,value.soilDryFull}) stream<<v<<' ';
        for(const auto* rules:{&value.rainRuleOutputs,&value.fogRuleOutputs,&value.dustRuleOutputs})
            for(double v:*rules) stream<<v<<' ';
    };
    append(fuzzy,f);append(original,defaults);
    if(fuzzy.str()!=original.str()) out<<"fuzzy1 "<<fuzzy.str();
    std::uint64_t key=14695981039346656037ull;
    for(unsigned char c:out.str()) {key^=c;key*=1099511628211ull;}return key;
}
}

void InstanceWeather::save(std::ostream& out) const {
    out<<std::setprecision(17)<<"CLIMATE1 "<<roomId_<<' '<<revision_<<' '<<profileKey(profile_)<<' ';
    for(double v:{simulationTimeSeconds_,pendingSimulationSeconds_,weatherPhase_,worldSimulationSeconds_,
        importedWaterKgM2_,exportedWaterKgM2_,nearAir_.temperatureC,nearAir_.depthM,nearAir_.vaporKgM2,nearAir_.liquidKgM2,
        upperAir_.temperatureC,upperAir_.depthM,upperAir_.vaporKgM2,upperAir_.liquidKgM2,
        ground_.temperatureC,ground_.waterKgM2,ground_.soilKgM2,ground_.snowKgM2,ground_.iceKgM2,ground_.filmKgM2,
        pressureHpa_,windSpeedMps_,windDirectionDegrees_,precipitationMmPerHour_,initialWaterKgM2_,drainedWaterKgM2_,
        environment_.waveHeightM,environment_.sandstormIntensity,environment_.surfaceHeatFluxWm2}) out<<v<<' ';
    out<<'\n';
}
bool InstanceWeather::restore(std::istream& in) {
    // 검증이 끝나기 전에는 현재 상태를 바꾸지 않아요. 잘린 파일도 부분 복구하지 않아요.
    auto next=*this;std::string version;std::uint32_t room=0;std::uint64_t key=0;
    if(!(in>>version>>room>>next.revision_>>key) || version!="CLIMATE1" || room!=roomId_ || key!=profileKey(profile_)) return false;
    const std::array<double*,29> fields={&next.simulationTimeSeconds_,&next.pendingSimulationSeconds_,&next.weatherPhase_,&next.worldSimulationSeconds_,
        &next.importedWaterKgM2_,&next.exportedWaterKgM2_,&next.nearAir_.temperatureC,&next.nearAir_.depthM,&next.nearAir_.vaporKgM2,&next.nearAir_.liquidKgM2,
        &next.upperAir_.temperatureC,&next.upperAir_.depthM,&next.upperAir_.vaporKgM2,&next.upperAir_.liquidKgM2,
        &next.ground_.temperatureC,&next.ground_.waterKgM2,&next.ground_.soilKgM2,&next.ground_.snowKgM2,&next.ground_.iceKgM2,&next.ground_.filmKgM2,
        &next.pressureHpa_,&next.windSpeedMps_,&next.windDirectionDegrees_,&next.precipitationMmPerHour_,&next.initialWaterKgM2_,&next.drainedWaterKgM2_,
        &next.environment_.waveHeightM,&next.environment_.sandstormIntensity,&next.environment_.surfaceHeatFluxWm2};
    for(auto* v:fields) if(!(in>>*v) || !std::isfinite(*v) || std::abs(*v)>1e14) return false;
    for(double v:{next.simulationTimeSeconds_,next.pendingSimulationSeconds_,next.importedWaterKgM2_,next.exportedWaterKgM2_,
        next.nearAir_.vaporKgM2,next.nearAir_.liquidKgM2,next.upperAir_.vaporKgM2,next.upperAir_.liquidKgM2,
        next.ground_.waterKgM2,next.ground_.soilKgM2,next.ground_.snowKgM2,next.ground_.iceKgM2,next.ground_.filmKgM2,
        next.initialWaterKgM2_,next.drainedWaterKgM2_,next.windSpeedMps_,next.precipitationMmPerHour_}) if(v<0) return false;
    if(next.nearAir_.depthM!=120 || next.upperAir_.depthM!=500 || next.ground_.soilKgM2>profile_.environment.soilCapacityKgM2 ||
        next.ground_.filmKgM2>profile_.environment.surfaceFilmCapacityKgM2 || next.environment_.sandstormIntensity<0 || next.environment_.sandstormIntensity>1) return false;
    next.updateClock();next.updateCoast(0);
    if(std::abs(next.snapshot().waterBalanceErrorKgM2)>.01) return false;
    *this=std::move(next);return true;
}
}
