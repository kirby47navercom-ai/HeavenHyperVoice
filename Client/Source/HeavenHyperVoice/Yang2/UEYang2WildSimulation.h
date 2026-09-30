#pragma once
// YANG2_CLIENT_AUTHORITY_ONLY: 월드/소켓 없이 실제 서버 AI를 실행하는 로컬 모델이에요.
#include "../Net/HHVFieldConnection.h"
#include "WildAi.h"
#include "InstanceWeather.h"
#include <memory>
#include <random>

class FYang2WildSimulation {
public:
    FYang2WildSimulation(const std::string& collision,const std::string& script,std::uint64_t expectedHash,
        std::uint32_t type,heaven::nav::Vec3 center,float extent,int count,
        heaven::instance::InstanceWeatherProfile profile,std::vector<std::uint16_t> pool);
    void Step(float dt,const heaven::instance::ObservedPlayer& player,const heaven::instance::InstanceWeatherSnapshot& climate,
        TArray<FHHVFieldEntity>& spawned,TArray<FHHVFieldEntity>& moved,TArray<uint64>& gone);
    bool SetHealth(std::uint64_t id,std::uint16_t hp);
    std::size_t AliveCount() const;
private:
    struct Slot {
        FHHVFieldEntity Entity;
        float Accumulator=0;
        double Respawn=0;
        bool Alive=false;
    };
    bool Spawn(Slot& slot,const heaven::instance::InstanceWeatherSnapshot& climate);
    heaven::Map Map;
    std::unique_ptr<heaven::instance::WildAi> Ai;
    heaven::instance::InstanceWeatherProfile Profile;
    heaven::instance::WildArea Area;
    std::vector<std::uint16_t> Pool;
    std::vector<Slot> Slots;
    std::mt19937 Random;
    std::uint32_t Type=0;
    double Time=0;
};
