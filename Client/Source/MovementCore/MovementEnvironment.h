#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace hhv::movement {
// 좌표는 언리얼과 같은 cm예요. 에디터 DA에서 서버 설정으로 내보내요.
struct WaterRegion {
    float minX=0,minY=0,maxX=0,maxY=0,seaLevelCm=0,swimDepthCm=90;
    bool canSwim=true;
    bool contains(float x,float y) const { return x>=minX && x<=maxX && y>=minY && y<=maxY; }
};
struct Environment {
    float speedMultiplier=1,traction=1,tideOffsetCm=0,swimSpeed=160;
    std::vector<WaterRegion> water;
};
inline bool valid(const WaterRegion& r) {
    return std::isfinite(r.minX) && std::isfinite(r.minY) && std::isfinite(r.maxX) &&
        std::isfinite(r.maxY) && std::isfinite(r.seaLevelCm) && std::isfinite(r.swimDepthCm) &&
        r.minX<r.maxX && r.minY<r.maxY && r.swimDepthCm>=20 && r.swimDepthCm<=300;
}
inline bool valid(const Environment& value) {
    return std::isfinite(value.speedMultiplier) && value.speedMultiplier>=.1f && value.speedMultiplier<=1 &&
        std::isfinite(value.traction) && value.traction>=.02f && value.traction<=1 &&
        std::isfinite(value.tideOffsetCm) && std::abs(value.tideOffsetCm)<=2000 &&
        std::isfinite(value.swimSpeed) && value.swimSpeed>=10 && value.swimSpeed<=1000 && value.water.size()<=64 &&
        std::all_of(value.water.begin(),value.water.end(),[](const auto& region){return valid(region);});
}
}
