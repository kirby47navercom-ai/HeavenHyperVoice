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
    p.environment.daySeconds=0;p.environment.dustFullWindMps=0;
    wet.initialize(1,47,p);wet.advance(1);
    require(std::isfinite(wet.snapshot().environment.sunElevationDegrees),"invalid profile made NaN");
    // 실제 서버가 읽는 형식으로 정상 설정과 잘못된 설정을 검사한다.
    const auto path=std::filesystem::temp_directory_path()/"hhv-environment-check.ini";
    {std::ofstream f(path);f<<"meanTemperatureC=32\ninitialSoilWaterKgM2=.3\nsandAvailability=1\n";}
    const auto loaded=loadEnvironmentProfile(path.string());
    require(loaded.meanTemperatureC==32 && loaded.environment.sandAvailability==1,"profile import differs");
    for(const char* invalid:{"daySeconds=0\n","typo=3\n","baseWindMps=nan\n","daySeconds=100\ndaySeconds=200\n"}) {
        {std::ofstream f(path);f<<invalid;}
        bool rejected=false;try{loadEnvironmentProfile(path.string());}catch(const std::exception&){rejected=true;}
        require(rejected,"invalid config accepted");
    }
    std::filesystem::remove(path);
    std::cout<<"Earth environment checks passed\n";
}
