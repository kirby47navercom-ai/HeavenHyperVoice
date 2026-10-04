// YANG2_CLIENT_AUTHORITY_ONLY
#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "Local wild simulation must not compile on main."
#endif
#include "UEYang2WildSimulation.h"
#include "WildBt.h"
#include "PokemonSpecies.h"
#include "PokemonMovement.h"
#include "EarthScience/EnvironmentMovement.h"
#include <stdexcept>

namespace {
std::shared_ptr<heaven::Map> LoadYang2Map(const std::string& file,std::uint64_t hash) {
    auto map=std::make_shared<heaven::Map>(0);std::string error;
    if(!map->loadFromFile(file,error)) throw std::runtime_error(error);
    if(map->collision().hash()!=hash) throw std::runtime_error("Offline AI/player collision hash mismatch");
    return map;
}
}

FYang2WildSimulation::FYang2WildSimulation(const std::string& collision,const std::string& script,std::uint64_t expectedHash,
    std::uint32_t type,heaven::nav::Vec3 center,float extent,int count,
    heaven::instance::InstanceWeatherProfile profile,std::vector<std::uint16_t> pool)
    :FYang2WildSimulation(LoadYang2Map(collision,expectedHash),script,type,center,extent,count,std::move(profile),std::move(pool)) {}

FYang2WildSimulation::FYang2WildSimulation(std::shared_ptr<heaven::Map> map,const std::string& script,
    std::uint32_t type,heaven::nav::Vec3 center,float extent,int count,
    heaven::instance::InstanceWeatherProfile profile,std::vector<std::uint16_t> pool)
    :Map(std::move(map)),Profile(std::move(profile)),Pool(std::move(pool)),Random(std::random_device{}()),Type(type) {
    if(!Map || !Map->loaded()) throw std::runtime_error("Offline navigation is not ready");
    Area={center.x,center.y,std::clamp(extent,100.f,10000.f)};
    Ai=std::make_unique<heaven::instance::WildAi>(std::make_unique<heaven::instance::WildBt>(script));
    Ai->setMap(Map.get());Ai->setArea(Area);
    if(Pool.empty()) for(const auto& base:heaven::proto::kSpecies) if(heaven::proto::isWildSpawnable(base.dex)) Pool.push_back(base.id);
    Slots.resize(std::clamp(count,0,64));
    for(std::size_t i=0;i<Slots.size();++i) Slots[i].Entity.EntityId=(1ull<<52)+i;
}
bool FYang2WildSimulation::Spawn(Slot& slot,const heaven::instance::InstanceWeatherSnapshot& climate) {
    std::vector<double> Weights;
    for(auto Species:Pool) {
        double Weight=1;const auto* Base=heaven::proto::findSpecies(Species);
        for(const auto& Rule:Profile.spawnRules) if(Base && Rule.pokemonDex==Base->dex) {Weight=heaven::instance::environmentSpawnWeight(Rule,climate);break;}
        Weights.push_back(Weight);
    }
    if(Pool.empty() || !std::any_of(Weights.begin(),Weights.end(),[](double W){return W>0;})) return false;
    std::discrete_distribution<std::size_t> Choose(Weights.begin(),Weights.end());
    std::uniform_real_distribution<float> X(Area.centerX-Area.halfExtent,Area.centerX+Area.halfExtent),Y(Area.centerY-Area.halfExtent,Area.centerY+Area.halfExtent);
    for(int Attempt=0;Attempt<16;++Attempt) {
        heaven::nav::Vec3 Position;
        if(!Map->canStandAt(X(Random),Y(Random),Map->agent(),&Position)) continue;
        bool Deep=false;
        for(const auto& Region:Profile.environment.waterRegions)
            if(Region.contains(Position.x,Position.y) && Region.seaLevelCm+climate.environment.tideLevelM*100-(Position.z-Map->agent().halfHeight)>Region.swimDepthCm) Deep=true;
        if(Deep) continue;
        const auto* Base=heaven::proto::findSpecies(Pool[Choose(Random)]);if(!Base) return false;
        const auto Id=slot.Entity.EntityId;slot.Entity={};slot.Entity.EntityId=Id;slot.Entity.Species=Base->dex;
        slot.Entity.CoreState.position=Position;slot.Entity.CoreState.mode=hhv::movement::Mode::Grounded;
        slot.Entity.CurrentHP=slot.Entity.MaxHP=heaven::proto::computeStats(*Base,heaven::proto::kStarterLevel,{},{}).maxHp;
        slot.Entity.ServerTimeSeconds=Time;slot.Alive=true;slot.Accumulator=0;return true;
    }
    return false;
}
void FYang2WildSimulation::Step(float dt,const heaven::instance::ObservedPlayer& player,const heaven::instance::InstanceWeatherSnapshot& climate,
    TArray<FHHVFieldEntity>& spawned,TArray<FHHVFieldEntity>& moved,TArray<uint64>& gone) {
    const float Elapsed=std::clamp(dt,0.f,.25f);Time+=Elapsed;
    Ai->setVisibilityMultiplier(static_cast<float>(climate.environment.visibilityMultiplier));
    const std::vector<heaven::instance::ObservedPlayer> Players{player};
    auto Land=heaven::instance::movementEnvironment(Profile.environment,climate);Land.speedMultiplier=1;
    for(auto& Region:Land.water) Region.canSwim=false;
    for(auto& Slot:Slots) {
        auto& Entity=Slot.Entity;
        if(Slot.Alive && Entity.CurrentHP==0) {Slot.Alive=false;Slot.Respawn=Profile.environment.wildRespawnSeconds;Ai->forget(Entity.EntityId);gone.Add(Entity.EntityId);}
        if(!Slot.Alive) {
            Slot.Respawn=std::max(0.,Slot.Respawn-Elapsed);
            if(Slot.Respawn==0) {if(Spawn(Slot,climate)) spawned.Add(Entity);else Slot.Respawn=1;}
            continue;
        }
        const auto Position=Entity.CoreState.position;
        const auto* Base=heaven::proto::findSpeciesByDex(Entity.Species);if(!Base) continue;
        const auto Intent=Ai->decide(Entity.EntityId,Base->id,Type,Position.x,Position.y,Position.z,Elapsed,Players);
        Map->advance(Entity.CoreState,Slot.Accumulator,Elapsed,{Intent.targetX,Intent.targetY,Position.z},Intent.moving,
            heaven::fieldshared::pokemonMoveSpeed(Base->id)*static_cast<float>(climate.environment.movementMultiplier),Land);
        if(Intent.moving && hhv::movement::length(Entity.CoreState.position-Position)<.01f) Ai->notifyMoveBlocked(Entity.EntityId);
        if(Intent.attacking && Intent.attackTargetId==player.entityId && std::hypot(Position.x-player.x,Position.y-player.y)<=Intent.attackRange) {
            Entity.AttackTargetId=player.entityId;++Entity.AttackSequence;
            Entity.CoreState.facing=std::atan2(player.y-Position.y,player.x-Position.x)*57.295779513f;
        }
        Entity.ServerTimeSeconds=Time;moved.Add(Entity);
    }
}
bool FYang2WildSimulation::SetHealth(std::uint64_t id,std::uint16_t hp) {
    for(auto& Slot:Slots) if(Slot.Alive && Slot.Entity.EntityId==id) {Slot.Entity.CurrentHP=std::min(hp,Slot.Entity.MaxHP);return true;}
    return false;
}
std::size_t FYang2WildSimulation::AliveCount() const {
    return std::count_if(Slots.begin(),Slots.end(),[](const auto& Slot){return Slot.Alive;});
}
