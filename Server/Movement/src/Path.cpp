#include "Path.h"

namespace heaven {

PathResult Pathfinder::find(const Map &map, const nav::Vec3 &start, const nav::Vec3 &goal,
                            const nav::Agent &agent) const {
    PathResult result;
    const auto &bakedAgent = map.agent();
    if (!map.loaded() || agent.radius != bakedAgent.radius || agent.halfHeight != bakedAgent.halfHeight ||
        agent.maxStepHeight != bakedAgent.maxStepHeight ||
        agent.maxSlopeAngleDegrees != bakedAgent.maxSlopeAngleDegrees) {
        return result;
    }

    std::vector<nav::Vec3> corePoints;
    if (!map.navigation().findPath(map.toCore(start), map.toCore(goal), agent.maxSearchNodes, corePoints)) {
        return result;
    }

    result.points.reserve(corePoints.size());
    for (const auto point : corePoints) {
        result.points.push_back(map.toServer(point));
    }
    result.found = true;
    return result;
}

} // namespace heaven
