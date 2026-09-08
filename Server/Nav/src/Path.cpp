#include "Path.h"

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <algorithm>
#include <array>
#include <vector>

namespace heaven {

namespace {

constexpr int kMaxStraightPathPoints = 128;

bool nearestRef(const Map& map, const dtNavMeshQuery& query, const nav::Vec3& position, const nav::Agent& agent,
                dtPolyRef& outRef, std::array<float, 3>& outPoint) {
    const dtQueryFilter* filter = map.queryFilter();
    nav::Vec3 grounded;
    if (filter == nullptr || !map.canStandAt(position.x, position.y, agent, &grounded, position.z)) {
        return false;
    }

    grounded.z -= agent.halfHeight;
    const std::array<float, 3> center = Map::toDetour(grounded);
    const std::array<float, 3> extents{0.5f, std::max(agent.height(), 300.f), 0.5f};
    outRef = 0;
    outPoint = {};
    return !dtStatusFailed(query.findNearestPoly(
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

    // A map is shared by rooms on different tick threads. findPath mutates
    // its node pool, so only the immutable navmesh may be shared.
    dtNavMeshQuery query;
    const int maxPolys = std::clamp(agent.maxSearchNodes, 64, 8192);
    if (dtStatusFailed(query.init(map.navMesh(), maxPolys))) {
        return result;
    }

    dtPolyRef startRef = 0;
    dtPolyRef goalRef = 0;
    std::array<float, 3> nearestStart{};
    std::array<float, 3> nearestGoal{};
    if (!nearestRef(map, query, start, agent, startRef, nearestStart) ||
        !nearestRef(map, query, goal, agent, goalRef, nearestGoal)) {
        return result;
    }

    const dtQueryFilter* filter = map.queryFilter();
    std::vector<dtPolyRef> polys(static_cast<std::size_t>(maxPolys));
    int polyCount = 0;
    const dtStatus pathStatus = query.findPath(startRef, goalRef, nearestStart.data(),
                                                nearestGoal.data(), filter, polys.data(),
                                                &polyCount, maxPolys);
    if (dtStatusFailed(pathStatus) || polyCount <= 0 || polys[polyCount - 1] != goalRef) {
        return result;
    }

    std::array<float, kMaxStraightPathPoints * 3> straight{};
    std::array<unsigned char, kMaxStraightPathPoints> flags{};
    std::array<dtPolyRef, kMaxStraightPathPoints> refs{};
    int straightCount = 0;
    const dtStatus straightStatus = query.findStraightPath(
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
        nav::Vec3 point = Map::fromDetour(&straight[static_cast<std::size_t>(i) * 3u]);
        point.z += agent.halfHeight;
        result.points.push_back(point);
    }

    result.found = !result.points.empty();
    return result;
}

}  // namespace heaven
