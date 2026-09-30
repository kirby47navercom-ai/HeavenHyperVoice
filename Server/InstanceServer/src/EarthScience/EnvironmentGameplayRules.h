#pragma once
#include <cstdint>
namespace heaven::instance {
struct InstanceWeatherSnapshot;
struct EnvironmentProfile;
// 종족/조건은 DA에 넣어요. 코드에 특정 포켓몬이나 좌표를 고정하지 않아요.
struct EnvironmentSpawnRule {
    std::uint16_t pokemonDex=0;
    double baseWeight=1, rainMultiplier=1, snowMultiplier=1, nightMultiplier=1;
};
double environmentSpawnWeight(const EnvironmentSpawnRule& rule,const InstanceWeatherSnapshot& weather);
void applyEnvironmentGameplay(const EnvironmentProfile& profile,InstanceWeatherSnapshot& weather);
}
