#include "InstanceWeather.h"
#include "EarthScience/EnvironmentConfig.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
using namespace heaven::instance;
// 서버 없이 실행 가능한 계산 검사. 실제 플레이 접속/멀티플레이 성능 검사를 대신하지 않는다.
static void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    if(argc>1) for(const auto& file:std::filesystem::directory_iterator(argv[1])) {
        if(file.path().extension()==".ini") {
            InstanceWeather sample;sample.initialize(1,47,loadEnvironmentProfile(file.path().string()));
            sample.advance(1);require(std::isfinite(sample.snapshot().temperatureC),"exported profile is invalid");
        }
    }
    InstanceWeatherProfile p; p.gameSecondsPerRealSecond=60;
    p.environment.tideAmplitudeM=2; p.environment.sandAvailability=1;
    p.environment.baseWindMps=14; p.environment.gustAmplitudeMps=0;
    p.initialSoilWaterKgM2=0; p.initialSurfaceWaterKgM2=0;
    InstanceWeather dry; dry.initialize(1,47,p);
    const auto initial=dry.snapshot();
    double tideMin=0,tideMax=0,sunMin=90,sunMax=-90,stormMax=0;
    for(int i=0;i<2880;++i) {
        dry.advance(1); const auto s=dry.snapshot(); const auto& e=s.environment;
        require(std::abs(s.waterBalanceErrorKgM2)<.001,"water not conserved");
        require(e.dayFraction>=0 && e.dayFraction<1,"invalid clock");
        require(std::abs(e.tideLevelM)<=2.0001,"tide outside amplitude");
        require(e.sandstormIntensity>=0 && e.sandstormIntensity<=1,"invalid dust");
        tideMin=std::min(tideMin,e.tideLevelM); tideMax=std::max(tideMax,e.tideLevelM);
        sunMin=std::min(sunMin,e.sunElevationDegrees); sunMax=std::max(sunMax,e.sunElevationDegrees);
        stormMax=std::max(stormMax,e.sandstormIntensity);
    }
    require(tideMin< -1.9 && tideMax>1.9,"missing tide cycle");
    require(sunMin< -10 && sunMax>10,"missing day/night");
    require(stormMax>.1,"dry windy desert never develops dust");
    require(dry.snapshot().environment.yearFraction!=initial.environment.yearFraction,"season is frozen");
    // 같은 총 경과시간을 다른 간격으로 넣어도 고정 계산 단계 결과가 같아야 한다.
    InstanceWeather a,b;a.initialize(1,47,p);b.initialize(1,47,p);
    for(int i=0;i<100;++i)a.advance(.1);
    for(int i=0;i<10;++i)b.advance(1);
    require(std::abs(a.snapshot().simulationTimeSeconds-b.snapshot().simulationTimeSeconds)<.01,"lost fractional time");
    require(std::abs(a.snapshot().temperatureC-b.snapshot().temperatureC)<.001,"frame dependent climate");
    p.initialSoilWaterKgM2=p.environment.soilCapacityKgM2;
    p.initialSurfaceWaterKgM2=20;
    InstanceWeather wet;wet.initialize(1,47,p);
    for(int i=0;i<120;++i)wet.advance(1);
    require(wet.snapshot().environment.sandstormIntensity<.01,"wet ground emits dust");
    // 토양이 젖어 있어도 표면 물막이 없으면 반짝이는 젖은 바닥으로 표시하지 않아요.
    InstanceWeatherProfile soil; soil.initialSurfaceWaterKgM2=0; soil.initialRelativeHumidityPct=0;
    InstanceWeather drySurface; drySurface.initialize(1,47,soil);
    require(drySurface.snapshot().groundWetness==0,"soil moisture leaks into visible wetness");
    require(drySurface.snapshot().relativeHumidityPct==0,"dry humidity is silently clamped");
    require(drySurface.snapshot().cloudCover==0,"dry climate starts with forced clouds");
    // 조석이 없는 호수/바다에서도 파도가 만들어져야 해요.
    soil.environment.baseWindMps=14; soil.environment.gustAmplitudeMps=0;
    drySurface.initialize(1,47,soil); drySurface.advance(10);
    require(drySurface.snapshot().environment.waveHeightM>0,"waves incorrectly depend on tide amplitude");
    // 방을 새로 만들거나 잠시 비워도 공용 시간은 같아요. 물순환 나이는 별개예요.
    InstanceWeather first,later; first.initialize(1,47,soil,100); later.initialize(1,48,soil,500);
    first.synchronizeClock(500);
    require(first.snapshot().environment.dayFraction==later.snapshot().environment.dayFraction,"rooms have separate world clocks");
    soil.environment.tideAmplitudeM=2;
    first.initialize(1,47,soil);
    require(std::abs(first.snapshot().environment.tideEnvelopeM-2)<1e-8,"spring tide amplitude differs");
    first.synchronizeClock(soil.environment.springNeapPeriodDays*soil.environment.daySeconds/soil.gameSecondsPerRealSecond/2);
    require(std::abs(first.snapshot().environment.tideEnvelopeM-1)<1e-8,"neap tide envelope missing");
    // 낮 일사/밤 냉각과 개방된 대기 수분의 보존을 확인해요.
    soil.environment.startHour=12; first.initialize(1,47,soil); first.advance(1);
    soil.environment.startHour=0; later.initialize(1,47,soil); later.advance(1);
    require(first.snapshot().environment.surfaceHeatFluxWm2>later.snapshot().environment.surfaceHeatFluxWm2,"solar energy is not applied");
    require(first.snapshot().environment.exportedWaterKgM2>0,"boundary moisture does not leave the room");
    require(std::abs(first.snapshot().waterBalanceErrorKgM2)<.001,"open atmosphere loses water accounting");
    InstanceWeatherSnapshot game; game.precipitationMmPerHour=10; game.temperatureC=20;
    game.environment.sunElevationDegrees=-12; game.groundWetness=1; game.environment.iceMm=1;
    game.environment.sandstormIntensity=1;
    require(std::abs(environmentSpawnWeight({393,2,3,4,5},game)-30)<.001,"rain/night spawn weighting differs");
    applyEnvironmentGameplay(soil.environment,game);
    require(game.environment.movementMultiplier<1 && game.environment.visibilityMultiplier<1,"gameplay multipliers are unused");
    p.environment.daySeconds=0;p.environment.dustFullWindMps=0;
    wet.initialize(1,47,p);wet.advance(1);
    require(std::isfinite(wet.snapshot().environment.sunElevationDegrees),"invalid profile made NaN");
    // 실제 서버가 읽는 형식으로 정상 설정과 잘못된 설정을 검사한다.
    const auto path=std::filesystem::temp_directory_path()/"hhv-environment-check.ini";
    {std::ofstream f(path);f<<"meanTemperatureC=32\ninitialSoilWaterKgM2=.3\nsandAvailability=1\nspawn.393=1,2,3,4\n";}
    const auto loaded=loadEnvironmentProfile(path.string());
    require(loaded.meanTemperatureC==32 && loaded.environment.sandAvailability==1,"profile import differs");
    require(loaded.spawnRules.size()==1 && loaded.spawnRules.front().rainMultiplier==2,"spawn rule import differs");
    for(const char* invalid:{"daySeconds=0\n","typo=3\n","baseWindMps=nan\n","daySeconds=100\ndaySeconds=200\n",
            "spawn.393=1,2,3\n","spawn.393=1,-2,3,4\n","spawn.65535=1,1,1,1\n"}) {
        {std::ofstream f(path);f<<invalid;}
        bool rejected=false;try{loadEnvironmentProfile(path.string());}catch(const std::exception&){rejected=true;}
        require(rejected,"invalid config accepted");
    }
    std::filesystem::remove(path);
    std::cout<<"Earth environment checks passed\n";
}
