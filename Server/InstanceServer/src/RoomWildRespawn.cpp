#include "RoomManager.h"
#include "InstanceGeometry.h"
#include "PokemonSpecies.h"
#include "EarthScience/EnvironmentMovement.h"
#include <algorithm>
namespace heaven::instance {
void RoomManager::respawnWild(Room& room,float dt) {
    if(!room.ai) return;
    const auto& type=types_.at(room.type);const auto& area=room.wildArea;
    bool due=false;
    for(std::size_t i=0;i<room.wildRespawnRemaining.size();++i) {
        auto& remaining=room.wildRespawnRemaining[i];
        if(remaining<0) {
            if(!room.world.retireWild(kWildIdBase+i)) continue;
            room.ai->forget(kWildIdBase+i);remaining=type.weather.environment.wildRespawnSeconds;
        }
        remaining=std::max(0.,remaining-std::max(0.f,dt));due=due || remaining==0;
    }
    if(!due) return; // 살아 있는 슬롯만 있으면 난수 분포/날씨 사본을 매 틱 만들지 않아요.
    const auto climate=room.weather.snapshot();
    std::vector<std::uint16_t> pool=type.wildSpecies;
    if(pool.empty()) for(const auto& base:proto::kSpecies) if(proto::isWildSpawnable(base.dex)) pool.push_back(base.id);
    std::vector<double> weights;
    for(auto species:pool) {
        double weight=1;const auto* base=proto::findSpecies(species);
        for(const auto& rule:type.weather.spawnRules) if(base && rule.pokemonDex==base->dex) {weight=environmentSpawnWeight(rule,climate);break;}
        weights.push_back(weight);
    }
    const bool available=std::any_of(weights.begin(),weights.end(),[](double value){return value>0;});
    std::discrete_distribution<std::size_t> choose(weights.begin(),weights.end());
    std::uniform_real_distribution<float> x(area.centerX-area.halfExtent,area.centerX+area.halfExtent),y(area.centerY-area.halfExtent,area.centerY+area.halfExtent);
    for(std::size_t i=0;i<room.wildRespawnRemaining.size();++i) {
        auto& remaining=room.wildRespawnRemaining[i];const auto id=kWildIdBase+i;
        if(remaining!=0 || !available || pool.empty()) continue;
        bool spawned=false;
        for(int attempt=0;attempt<16;++attempt) {
            nav::Vec3 location;if(!type.map->canStandAt(x(room.wildRandom),y(room.wildRandom),type.map->agent(),&location)) continue;
            const auto core=type.map->toCore(location);bool deep=false;
            for(const auto& region:type.weather.environment.waterRegions)
                if(region.contains(core.x,core.y) && region.seaLevelCm+climate.environment.tideLevelM*100-(core.z-type.map->agent().halfHeight)>region.swimDepthCm) deep=true;
            if(deep) continue; // 현재 야생은 육상 이동만 하므로 깊은 물에는 생성하지 않아요.
            room.world.enterWild(id,pool[choose(room.wildRandom)],{room.type,location.x,location.y,location.z,0});
            remaining=-1;spawned=true;break;
        }
        if(!spawned) remaining=1; // 실패했을 때 매 틱 무한 재탐색하지 않아요.
    }
}
}
