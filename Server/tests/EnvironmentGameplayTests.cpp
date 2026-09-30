#include "MovementPrediction.h"
#include "TriangleWorld.h"
#include "RoomManager.h"
#include "InstanceGeometry.h"
#include "EarthScience/EnvironmentCheckpoint.h"
#include "EarthScience/EnvironmentMovement.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
using namespace hhv::movement;
using namespace heaven::instance;
static void check(bool pass,const char* message) {if(!pass) throw std::runtime_error(message);}
static void runTests(int argc,char** argv) {
    check(argc==2,"AI script argument required");
    TriangleWorld terrain;terrain.build({{{-6000,-6000,0},{6000,-6000,0},{6000,6000,0}},{{-6000,-6000,0},{6000,6000,0},{-6000,6000,0}}});
    Config normal,ice;ice.environment.traction=.12f;
    State a,b;a.position=b.position={0,0,88.1f};a.mode=b.mode=Mode::Grounded;a.velocity=b.velocity={260,0,0};
    simulate(a,{},normal,terrain);simulate(b,{},ice,terrain);
    check(b.velocity.x>a.velocity.x,"ice must brake more slowly");
    Config water;water.environment.water.push_back({-1000,-1000,1000,1000,200,90,true});
    a={};a.position={0,0,88.1f};a.mode=Mode::Grounded;
    check(std::abs(waterDepth(a.position,water,terrain)-200)<1,"depth must use actual floor");
    simulate(a,{0,1,0,Run|Roll|Jump},water,terrain);
    check(a.mode==Mode::Swimming && a.rollRemaining==0 && length({a.velocity.x,a.velocity.y,0})<=water.environment.swimSpeed,"water did not cancel run/roll");
    for(int i=0;i<120;++i) simulate(a,{},water,terrain);
    check(a.mode==Mode::Swimming,"surface swimming oscillates out of water");
    water.environment.tideOffsetCm=-190;simulate(a,{},water,terrain);
    check(a.mode!=Mode::Swimming,"low tide did not release swimming");
    water.environment.tideOffsetCm=0;water.environment.water.front().canSwim=false;
    a={};a.position={1001,0,88.1f};a.mode=Mode::Grounded;a.velocity={-260,0,0};const auto before=a.position;
    simulate(a,{0,-1,0,0},water,terrain);check(a.position.x==before.x,"non-swimmable deep shore allowed entry");
    // 지연된 서버 보정은 같은 환경으로 미확인 입력을 다시 재생해야 해요.
    PredictionQueue prediction;State initial;initial.position={0,0,88.1f};initial.mode=Mode::Grounded;prediction.reset(initial);
    prediction.predict({0,1,0,0},normal,terrain);prediction.predict({0,1,0,0},normal,terrain);prediction.takeUnsent();
    State authoritative=initial;simulate(authoritative,{1,1,0,0},ice,terrain);
    check(prediction.acknowledge(1,authoritative,terrain),"environment correction rejected");
    simulate(authoritative,{2,1,0,0},ice,terrain);
    check(length(prediction.state.position-authoritative.position)<.001f,"ice prediction did not replay server environment");
    State swimmer=initial;std::deque<StepRecord> replay;
    for(int i=0;i<20;++i) {StepRecord step;step.input.sequence=i+1;step.input.x=1;step.config=water;step.config.environment.water.front().canSwim=true;
        step.before=swimmer;simulate(swimmer,step.input,step.config,terrain);step.after=swimmer;replay.push_back(step);}
    std::stringstream recording;check(saveReplay(recording,terrain.hash(),replay),"water replay save failed");std::size_t verified=0;
    check(verifyReplay(recording,terrain,verified) && verified==replay.size(),"water environment missing in replay");
    // 실제 방 생성/리스폰/재시작 복구 경로를 소켓 없이 검사해요.
    const auto fixture=std::filesystem::temp_directory_path()/"hhv-environment-gameplay.hhvcollision";
    {std::ofstream out(fixture);check(terrain.save(out),"fixture save failed");}
    heaven::Map map(kWorldOriginOffset);std::string error;check(map.loadFromFile(fixture.string(),error),error.c_str());
    RoomSettings settings;settings.wildPerRoom=1;settings.wildSeed=123;settings.wildAiScript=argv[1];
    InstanceType type;type.map=&map;type.wildSpecies={heaven::proto::findSpeciesByDex(393)->id};
    type.weather.environment.wildRespawnSeconds=1;
    RoomManager rooms(settings,{{1,type}});auto* room=rooms.join(1);check(room->world.size()==1,"initial weighted spawn missing");
    rooms.tickShard(0,1,2);check(room->world.setWildCurrentHp(kWildIdBase,0),"death API failed");
    rooms.tickShard(0,1,.1f);check(room->world.size()==0,"dead wild not retired");
    rooms.tickShard(0,1,1);check(room->world.size()==1,"wild respawn missing");
    const auto old=room->weather.snapshot();const auto saved=rooms.saveEnvironment();
    RoomManager restored(settings,{{1,type}});restored.restoreEnvironment(saved);auto* back=restored.join(1,room->id);
    check(back->id==room->id && back->weather.snapshot().temperatureC==old.temperatureC,"restored room climate differs");
    room->weather.advance(2);back->weather.advance(2);
    check(room->weather.snapshot().snowDepthM==back->weather.snapshot().snowDepthM &&
        room->weather.snapshot().relativeHumidityPct==back->weather.snapshot().relativeHumidityPct,"internal climate storage did not resume identically");
    const auto file=std::filesystem::temp_directory_path()/"hhv-environment-gameplay.state";
    writeEnvironmentCheckpoint(file,saved);writeEnvironmentCheckpoint(file,saved);
    {std::ofstream out(file,std::ios::trunc);out<<"broken";}
    check(readEnvironmentCheckpoint(file)==saved,"corrupted save did not recover backup");
    bool rejected=false;try{restored.restoreEnvironment(saved.substr(0,saved.size()/2));}catch(const std::exception&){rejected=true;}
    check(rejected,"truncated climate accepted");
    std::filesystem::remove(fixture);std::filesystem::remove(file);std::filesystem::remove(file.string()+".bak");
    std::cout<<"Environment gameplay checks passed\n";
}
int main(int argc,char** argv) {
    try {runTests(argc,argv);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
