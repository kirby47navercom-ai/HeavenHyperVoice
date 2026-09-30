#include "EnvironmentConfig.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <sstream>
#include "PokemonSpecies.h"
namespace heaven::instance {
InstanceWeatherProfile loadEnvironmentProfile(const std::string& filename) {
    // 데이터 에셋과 같은 필드 이름을 사용하므로 특정 맵 경로/좌표를 코드에 박지 않는다.
    InstanceWeatherProfile result;
    const std::map<std::string,double*> fields={
        {"iceTractionMultiplier",&result.environment.iceTractionMultiplier},
        {"swimSpeedCmPerSecond",&result.environment.swimSpeedCmPerSecond},
        {"wildRespawnSeconds",&result.environment.wildRespawnSeconds},
        {"daySeconds",&result.environment.daySeconds}, {"yearDays",&result.environment.yearDays},
        {"startHour",&result.environment.startHour}, {"startYearFraction",&result.environment.startYearFraction},
        {"latitudeDegrees",&result.environment.latitudeDegrees}, {"altitudeM",&result.environment.altitudeM},
        {"seasonalAmplitudeC",&result.environment.seasonalAmplitudeC}, {"dailyAmplitudeC",&result.environment.dailyAmplitudeC},
        {"thermalResponseSeconds",&result.environment.thermalResponseSeconds}, {"soilCapacityKgM2",&result.environment.soilCapacityKgM2},
        {"fieldCapacityKgM2",&result.environment.fieldCapacityKgM2}, {"infiltrationKgM2PerSecond",&result.environment.infiltrationKgM2PerSecond},
        {"drainageSeconds",&result.environment.drainageSeconds}, {"baseWindMps",&result.environment.baseWindMps},
        {"gustAmplitudeMps",&result.environment.gustAmplitudeMps}, {"sandAvailability",&result.environment.sandAvailability},
        {"dustStartWindMps",&result.environment.dustStartWindMps}, {"dustFullWindMps",&result.environment.dustFullWindMps},
        {"dustResponseSeconds",&result.environment.dustResponseSeconds}, {"tideAmplitudeM",&result.environment.tideAmplitudeM},
        {"tidePeriodHours",&result.environment.tidePeriodHours}, {"tidePhaseDegrees",&result.environment.tidePhaseDegrees},
        {"waveMaxHeightM",&result.environment.waveMaxHeightM}, {"waveResponseSeconds",&result.environment.waveResponseSeconds} ,
        {"surfaceFilmCapacityKgM2",&result.environment.surfaceFilmCapacityKgM2},
        {"moistureExchangeSeconds",&result.environment.moistureExchangeSeconds},
        {"solarPeakWm2",&result.environment.solarPeakWm2},
        {"surfaceAlbedo",&result.environment.surfaceAlbedo},
        {"surfaceEmissivity",&result.environment.surfaceEmissivity},
        {"clearSkyCoolingWm2",&result.environment.clearSkyCoolingWm2},
        {"airHeatTransferWm2K",&result.environment.airHeatTransferWm2K},
        {"springNeapPeriodDays",&result.environment.springNeapPeriodDays},
        {"neapTideFraction",&result.environment.neapTideFraction},
        {"springNeapPhaseDegrees",&result.environment.springNeapPhaseDegrees},
        {"shoreHeightM",&result.environment.shoreHeightM},
        {"wetMovementMultiplier",&result.environment.wetMovementMultiplier},
        {"iceMovementMultiplier",&result.environment.iceMovementMultiplier},
        {"sandVisibilityMultiplier",&result.environment.sandVisibilityMultiplier},
        {"meanTemperatureC",&result.meanTemperatureC},
        {"initialRelativeHumidityPct",&result.initialRelativeHumidityPct},
        {"meanPressureHpa",&result.meanPressureHpa},
        {"initialSurfaceWaterKgM2",&result.initialSurfaceWaterKgM2},
        {"initialSoilWaterKgM2",&result.initialSoilWaterKgM2},
        {"gameSecondsPerRealSecond",&result.gameSecondsPerRealSecond}
    };
    std::ifstream input(filename);
    if(!input) throw std::runtime_error("Cannot open environment profile: "+filename);
    std::set<std::string> seen;
    std::string line;
    while(std::getline(input,line)) {
        const auto comment=line.find('#'); if(comment!=std::string::npos) line.resize(comment);
        const auto first=line.find_first_not_of(" \t\r"); if(first==std::string::npos) continue;
        const auto equals=line.find('=',first);
        if(equals==std::string::npos) throw std::runtime_error("Expected name=value: "+line);
        const auto key=line.substr(first,line.find_last_not_of(" \t",equals-1)-first+1);
        if(!seen.insert(key).second) throw std::runtime_error("Duplicate environment key: "+key);
        if(key.rfind("water.",0)==0) {
            const auto id=key.substr(6);
            if(id.empty() || id.find_first_not_of("0123456789")!=std::string::npos || result.environment.waterRegions.size()>=64)
                throw std::runtime_error("Invalid/too many water regions: "+key);
            hhv::movement::WaterRegion r; int swimming=0;
            std::string text=line.substr(equals+1);std::replace(text.begin(),text.end(),',',' ');
            std::istringstream values(text);std::string extra;
            if(!(values>>r.minX>>r.minY>>r.maxX>>r.maxY>>r.seaLevelCm>>r.swimDepthCm>>swimming) || values>>extra ||
                !hhv::movement::valid(r) || (swimming!=0 && swimming!=1)) throw std::runtime_error("Invalid water region: "+line);
            r.canSwim=swimming!=0;
            for(const auto& old:result.environment.waterRegions)
                if(r.minX<old.maxX && r.maxX>old.minX && r.minY<old.maxY && r.maxY>old.minY)
                    throw std::runtime_error("Overlapping water regions: "+line);
            result.environment.waterRegions.push_back(r);continue;
        }
        if(key.rfind("spawn.",0)==0) {
            std::size_t end=0; const auto idText=key.substr(6);
            if(idText.empty() || idText.find_first_not_of("0123456789")!=std::string::npos)
                throw std::runtime_error("Invalid spawn dex: "+key);
            const auto dex=std::stoul(idText,&end);
            if(dex==0 || dex>65535 || !heaven::proto::findSpeciesByDex(static_cast<std::uint16_t>(dex)))
                throw std::runtime_error("Unknown spawn dex: "+key);
            EnvironmentSpawnRule rule; rule.pokemonDex=static_cast<std::uint16_t>(dex);
            if(std::any_of(result.spawnRules.begin(),result.spawnRules.end(),[&](const auto& r){return r.pokemonDex==rule.pokemonDex;}))
                throw std::runtime_error("Duplicate spawn dex: "+key);
            std::string text=line.substr(equals+1); std::replace(text.begin(),text.end(),',',' ');
            std::istringstream values(text); std::string extra;
            if(!(values>>rule.baseWeight>>rule.rainMultiplier>>rule.snowMultiplier>>rule.nightMultiplier) || values>>extra)
                throw std::runtime_error("Expected four spawn weights: "+line);
            for(double v:{rule.baseWeight,rule.rainMultiplier,rule.snowMultiplier,rule.nightMultiplier})
                if(!std::isfinite(v) || v<0 || v>100) throw std::runtime_error("Invalid spawn weight: "+line);
            result.spawnRules.push_back(rule); continue;
        }
        const auto field=fields.find(key);
        if(field==fields.end()) throw std::runtime_error("Unknown/duplicate environment key: "+key);
        const auto text=line.substr(equals+1); std::size_t end=0;
        const double value=std::stod(text,&end);
        if(!std::isfinite(value) || text.find_first_not_of(" \t\r",end)!=std::string::npos)
            throw std::runtime_error("Invalid environment value: "+line);
        *field->second=value;
    }
    auto safe=result.environment; normalizeEnvironment(safe);
    if(safe.iceTractionMultiplier!=result.environment.iceTractionMultiplier || safe.swimSpeedCmPerSecond!=result.environment.swimSpeedCmPerSecond ||
        safe.wildRespawnSeconds!=result.environment.wildRespawnSeconds) throw std::runtime_error("Invalid movement/respawn setting");
    if(safe.daySeconds!=result.environment.daySeconds) throw std::runtime_error("Invalid daySeconds");
    if(safe.yearDays!=result.environment.yearDays) throw std::runtime_error("Invalid yearDays");
    if(safe.startHour!=result.environment.startHour) throw std::runtime_error("Invalid startHour");
    if(safe.startYearFraction!=result.environment.startYearFraction) throw std::runtime_error("Invalid startYearFraction");
    if(safe.latitudeDegrees!=result.environment.latitudeDegrees) throw std::runtime_error("Invalid latitudeDegrees");
    if(safe.altitudeM!=result.environment.altitudeM) throw std::runtime_error("Invalid altitudeM");
    if(safe.seasonalAmplitudeC!=result.environment.seasonalAmplitudeC) throw std::runtime_error("Invalid seasonalAmplitudeC");
    if(safe.dailyAmplitudeC!=result.environment.dailyAmplitudeC) throw std::runtime_error("Invalid dailyAmplitudeC");
    if(safe.thermalResponseSeconds!=result.environment.thermalResponseSeconds) throw std::runtime_error("Invalid thermalResponseSeconds");
    if(safe.soilCapacityKgM2!=result.environment.soilCapacityKgM2) throw std::runtime_error("Invalid soilCapacityKgM2");
    if(safe.fieldCapacityKgM2!=result.environment.fieldCapacityKgM2) throw std::runtime_error("Invalid fieldCapacityKgM2");
    if(safe.infiltrationKgM2PerSecond!=result.environment.infiltrationKgM2PerSecond) throw std::runtime_error("Invalid infiltrationKgM2PerSecond");
    if(safe.drainageSeconds!=result.environment.drainageSeconds) throw std::runtime_error("Invalid drainageSeconds");
    if(safe.baseWindMps!=result.environment.baseWindMps) throw std::runtime_error("Invalid baseWindMps");
    if(safe.gustAmplitudeMps!=result.environment.gustAmplitudeMps) throw std::runtime_error("Invalid gustAmplitudeMps");
    if(safe.sandAvailability!=result.environment.sandAvailability) throw std::runtime_error("Invalid sandAvailability");
    if(safe.dustStartWindMps!=result.environment.dustStartWindMps) throw std::runtime_error("Invalid dustStartWindMps");
    if(safe.dustFullWindMps!=result.environment.dustFullWindMps) throw std::runtime_error("Invalid dustFullWindMps");
    if(safe.dustResponseSeconds!=result.environment.dustResponseSeconds) throw std::runtime_error("Invalid dustResponseSeconds");
    if(safe.tideAmplitudeM!=result.environment.tideAmplitudeM) throw std::runtime_error("Invalid tideAmplitudeM");
    if(safe.tidePeriodHours!=result.environment.tidePeriodHours) throw std::runtime_error("Invalid tidePeriodHours");
    if(safe.tidePhaseDegrees!=result.environment.tidePhaseDegrees) throw std::runtime_error("Invalid tidePhaseDegrees");
    if(safe.waveMaxHeightM!=result.environment.waveMaxHeightM) throw std::runtime_error("Invalid waveMaxHeightM");
    if(safe.waveResponseSeconds!=result.environment.waveResponseSeconds) throw std::runtime_error("Invalid waveResponseSeconds");
    if(safe.surfaceFilmCapacityKgM2!=result.environment.surfaceFilmCapacityKgM2) throw std::runtime_error("Invalid surfaceFilmCapacityKgM2");
    if(safe.moistureExchangeSeconds!=result.environment.moistureExchangeSeconds) throw std::runtime_error("Invalid moistureExchangeSeconds");
    if(safe.solarPeakWm2!=result.environment.solarPeakWm2) throw std::runtime_error("Invalid solarPeakWm2");
    if(safe.surfaceAlbedo!=result.environment.surfaceAlbedo) throw std::runtime_error("Invalid surfaceAlbedo");
    if(safe.surfaceEmissivity!=result.environment.surfaceEmissivity) throw std::runtime_error("Invalid surfaceEmissivity");
    if(safe.clearSkyCoolingWm2!=result.environment.clearSkyCoolingWm2) throw std::runtime_error("Invalid clearSkyCoolingWm2");
    if(safe.airHeatTransferWm2K!=result.environment.airHeatTransferWm2K) throw std::runtime_error("Invalid airHeatTransferWm2K");
    if(safe.springNeapPeriodDays!=result.environment.springNeapPeriodDays) throw std::runtime_error("Invalid springNeapPeriodDays");
    if(safe.neapTideFraction!=result.environment.neapTideFraction) throw std::runtime_error("Invalid neapTideFraction");
    if(safe.springNeapPhaseDegrees!=result.environment.springNeapPhaseDegrees) throw std::runtime_error("Invalid springNeapPhaseDegrees");
    if(safe.shoreHeightM!=result.environment.shoreHeightM) throw std::runtime_error("Invalid shoreHeightM");
    if(safe.wetMovementMultiplier!=result.environment.wetMovementMultiplier) throw std::runtime_error("Invalid wetMovementMultiplier");
    if(safe.iceMovementMultiplier!=result.environment.iceMovementMultiplier) throw std::runtime_error("Invalid iceMovementMultiplier");
    if(safe.sandVisibilityMultiplier!=result.environment.sandVisibilityMultiplier) throw std::runtime_error("Invalid sandVisibilityMultiplier");
    if(result.meanTemperatureC<-60 || result.meanTemperatureC>60) throw std::runtime_error("Invalid meanTemperatureC");
    if(result.initialRelativeHumidityPct<0 || result.initialRelativeHumidityPct>100) throw std::runtime_error("Invalid initialRelativeHumidityPct");
    if(result.meanPressureHpa<800 || result.meanPressureHpa>1100) throw std::runtime_error("Invalid meanPressureHpa");
    if(result.initialSurfaceWaterKgM2<0 || result.initialSurfaceWaterKgM2>1000) throw std::runtime_error("Invalid initialSurfaceWaterKgM2");
    if(result.initialSoilWaterKgM2<0 || result.initialSoilWaterKgM2>10000) throw std::runtime_error("Invalid initialSoilWaterKgM2");
    if(result.gameSecondsPerRealSecond<0 || result.gameSecondsPerRealSecond>3600) throw std::runtime_error("Invalid gameSecondsPerRealSecond");
    if(result.initialSoilWaterKgM2>result.environment.soilCapacityKgM2)
        throw std::runtime_error("Initial soil water exceeds soil capacity");
    return result;
}
}
