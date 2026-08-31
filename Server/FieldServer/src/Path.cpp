#include "Path.h"

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <algorithm>
#include <array>
#include <vector>

namespace heaven::field {

namespace {

constexpr int kMaxStraightPathPoints = 128;

bool nearestRef(const Map& map, const nav::Vec3& position, const nav::Agent& agent,
                dtPolyRef& outRef, std::array<float, 3>& outPoint) {
    const dtNavMeshQuery* query = map.navQuery();
    const dtQueryFilter* filter = map.queryFilter();
    if (query == nullptr || filter == nullptr) {
        return false;
    }

    const std::array<float, 3> center = Map::toDetour(position);
    const std::array<float, 3> extents = map.queryExtents(agent);
    outRef = 0;
    outPoint = {};
    return !dtStatusFailed(query->findNearestPoly(
               center.data(), extents.data(), filter, &outRef, outPoint.data())) &&
           outRef != 0;
}

}  // namespace

PathResult Pathfinder::find(const Map& map, const nav::Vec3& start, const nav::Vec3& goal,
                            const nav::Agent& agent) const {
    PathResult result;
    if (!map.hasNavMesh()) {
        return result;
    }

    dtPolyRef startRef = 0;
    dtPolyRef goalRef = 0;
    std::array<float, 3> nearestStart{};
    std::array<float, 3> nearestGoal{};
    if (!nearestRef(map, start, agent, startRef, nearestStart) ||
        !nearestRef(map, goal, agent, goalRef, nearestGoal)) {
        return result;
    }

    const dtNavMeshQuery* query = map.navQuery();
    const dtQueryFilter* filter = map.queryFilter();
    const int maxPolys = std::clamp(agent.maxSearchNodes, 64, 8192);
    std::vector<dtPolyRef> polys(static_cast<std::size_t>(maxPolys));
    int polyCount = 0;
    const dtStatus pathStatus = query->findPath(startRef, goalRef, nearestStart.data(),
                                                nearestGoal.data(), filter, polys.data(),
                                                &polyCount, maxPolys);
    if (dtStatusFailed(pathStatus) || polyCount <= 0 || polys[polyCount - 1] != goalRef) {
        return result;
    }

    std::array<float, kMaxStraightPathPoints * 3> straight{};
    std::array<unsigned char, kMaxStraightPathPoints> flags{};
    std::array<dtPolyRef, kMaxStraightPathPoints> refs{};
    int straightCount = 0;
    const dtStatus straightStatus = query->findStraightPath(
        nearestStart.data(),
        nearestGoal.data(),
        polys.data(),
        polyCount,
        straight.data(),
        flags.data(),
        refs.data(),
        &straightCount,
        kMaxStraightPathPoints);
    if (dtStatusFailed(straightStatus) || straightCount <= 0) {
        return result;
    }

    result.points.reserve(static_cast<std::size_t>(straightCount));
    for (int i = 0; i < straightCount; ++i) {
        result.points.push_back(Map::fromDetour(&straight[static_cast<std::size_t>(i) * 3u]));
    }

    result.found = !result.points.empty();
    return result;
}

}  // namespace heaven::field
