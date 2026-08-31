#pragma once

#include <vector>

#include "Map.h"

namespace heaven::field {

struct PathResult {
    bool found = false;
    std::vector<nav::Vec3> points;
};

class Pathfinder {
public:
    PathResult find(const Map& map, const nav::Vec3& start, const nav::Vec3& goal,
                    const nav::Agent& agent) const;
};

}  // namespace heaven::field
