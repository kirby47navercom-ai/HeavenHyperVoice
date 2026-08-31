#include "Map.h"

#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <Recast.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace heaven::field {

namespace {

constexpr unsigned char kGroundArea = 1;
constexpr unsigned short kWalkFlag = 1;
constexpr int kMaxRaycastPolys = 256;
constexpr int kMaxNearestPolys = 128;

struct HeightFieldDeleter {
    void operator()(rcHeightfield* value) const { rcFreeHeightField(value); }
};

struct CompactHeightFieldDeleter {
    void operator()(rcCompactHeightfield* value) const { rcFreeCompactHeightfield(value); }
};

struct ContourSetDeleter {
    void operator()(rcContourSet* value) const { rcFreeContourSet(value); }
};

struct PolyMeshDeleter {
    void operator()(rcPolyMesh* value) const { rcFreePolyMesh(value); }
};

struct PolyMeshDetailDeleter {
    void operator()(rcPolyMeshDetail* value) const { rcFreePolyMeshDetail(value); }
};

class BuildContext final : public rcContext {
public:
    BuildContext() : rcContext(false) {}

protected:
    void doLog(const rcLogCategory, const char*, const int) override {}
};

bool parseFloat(std::istringstream& stream, float& out) {
    stream >> out;
    return !stream.fail();
}

float sqr(float value) {
    return value * value;
}

bool sameToken(const std::string& a, const char* b) {
    return a == b;
}

void copyBounds(const nav::Aabb& bounds, float* bmin, float* bmax) {
    bmin[0] = bounds.min.x;
    bmin[1] = bounds.min.z;
    bmin[2] = bounds.min.y;
    bmax[0] = bounds.max.x;
    bmax[1] = bounds.max.z;
    bmax[2] = bounds.max.y;
}

}  // namespace

Map::Map() : filter_(new dtQueryFilter()) {
    if (filter_) {
        filter_->setIncludeFlags(kWalkFlag);
        filter_->setExcludeFlags(0);
    }
}

Map::~Map() = default;
Map::Map(Map&&) noexcept = default;
Map& Map::operator=(Map&&) noexcept = default;

void Map::NavMeshDeleter::operator()(dtNavMesh* mesh) const {
    dtFreeNavMesh(mesh);
}

void Map::NavMeshQueryDeleter::operator()(dtNavMeshQuery* query) const {
    dtFreeNavMeshQuery(query);
}

void Map::QueryFilterDeleter::operator()(dtQueryFilter* filter) const {
    delete filter;
}

void Map::clear() {
    source_.clear();
    bounds_ = {};
    agent_ = {};
    settings_ = {};
    triangles_.clear();
    groundTriangleCount_ = 0;
    wallTriangleCount_ = 0;
    walkablePolyCount_ = 0;
    loaded_ = false;
    query_.reset();
    navMesh_.reset();
}

bool Map::loadFromFile(const std::string& path, std::string& error) {
    clear();

    std::ifstream in(path);
    if (!in) {
        error = "cannot open " + path;
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (!parseLine(line, lineNumber, error)) {
            clear();
            return false;
        }
    }

    if (triangles_.empty()) {
        error = "no nav geometry triangles in " + path;
        clear();
        return false;
    }

    if (!bounds_.valid) {
        error = "nav geometry bounds are empty in " + path;
        clear();
        return false;
    }

    if (!filter_) {
        filter_.reset(new dtQueryFilter());
    }
    if (!filter_) {
        error = "cannot allocate Detour query filter";
        clear();
        return false;
    }
    filter_->setIncludeFlags(kWalkFlag);
    filter_->setExcludeFlags(0);

    if (!buildNavMesh(error)) {
        clear();
        return false;
    }

    loaded_ = true;
    return true;
}

bool Map::parseLine(const std::string& line, int lineNumber, std::string& error) {
    std::istringstream stream(line);
    std::string type;
    stream >> type;
    if (type.empty() || type[0] == '#') {
        return true;
    }

    if (sameToken(type, "source")) {
        stream >> source_;
        return true;
    }

    if (sameToken(type, "settings")) {
        if (!parseFloat(stream, settings_.cellSize) ||
            !parseFloat(stream, settings_.cellHeight) ||
            !parseFloat(stream, agent_.radius) ||
            !parseFloat(stream, agent_.halfHeight) ||
            !parseFloat(stream, agent_.maxStepHeight) ||
            !parseFloat(stream, agent_.maxSlopeAngleDegrees)) {
            error = "malformed settings at line " + std::to_string(lineNumber);
            return false;
        }
        return true;
    }

    if (sameToken(type, "bounds")) {
        if (!parseFloat(stream, bounds_.min.x) ||
            !parseFloat(stream, bounds_.min.y) ||
            !parseFloat(stream, bounds_.min.z) ||
            !parseFloat(stream, bounds_.max.x) ||
            !parseFloat(stream, bounds_.max.y) ||
            !parseFloat(stream, bounds_.max.z)) {
            error = "malformed bounds at line " + std::to_string(lineNumber);
            return false;
        }
        bounds_.valid = true;
        return true;
    }

    if (sameToken(type, "triangle")) {
        nav::Triangle triangle;
        std::string area;
        if (!parseFloat(stream, triangle.a.x) ||
            !parseFloat(stream, triangle.a.y) ||
            !parseFloat(stream, triangle.a.z) ||
            !parseFloat(stream, triangle.b.x) ||
            !parseFloat(stream, triangle.b.y) ||
            !parseFloat(stream, triangle.b.z) ||
            !parseFloat(stream, triangle.c.x) ||
            !parseFloat(stream, triangle.c.y) ||
            !parseFloat(stream, triangle.c.z) ||
            !(stream >> area)) {
            error = "malformed triangle at line " + std::to_string(lineNumber);
            return false;
        }

        if (area == "ground") {
            triangle.area = kGroundArea;
            ++groundTriangleCount_;
        } else if (area == "wall") {
            triangle.area = RC_NULL_AREA;
            ++wallTriangleCount_;
        } else {
            error = "unknown triangle area at line " + std::to_string(lineNumber);
            return false;
        }

        triangles_.push_back(triangle);
        updateBounds(triangle.a);
        updateBounds(triangle.b);
        updateBounds(triangle.c);
        return true;
    }

    error = "unknown map directive '" + type + "' at line " + std::to_string(lineNumber);
    return false;
}

void Map::updateBounds(const nav::Vec3& value) {
    if (!bounds_.valid) {
        bounds_.min = value;
        bounds_.max = value;
        bounds_.valid = true;
        return;
    }
    bounds_.min.x = std::min(bounds_.min.x, value.x);
    bounds_.min.y = std::min(bounds_.min.y, value.y);
    bounds_.min.z = std::min(bounds_.min.z, value.z);
    bounds_.max.x = std::max(bounds_.max.x, value.x);
    bounds_.max.y = std::max(bounds_.max.y, value.y);
    bounds_.max.z = std::max(bounds_.max.z, value.z);
}

bool Map::buildNavMesh(std::string& error) {
    BuildContext ctx;

    std::vector<float> vertices;
    vertices.reserve(triangles_.size() * 9u);
    std::vector<int> indices;
    indices.reserve(triangles_.size() * 3u);
    std::vector<unsigned char> areas;
    areas.reserve(triangles_.size());

    int nextIndex = 0;
    for (const nav::Triangle& triangle : triangles_) {
        const std::array<float, 3> a = toDetour(triangle.a);
        const std::array<float, 3> b = toDetour(triangle.b);
        const std::array<float, 3> c = toDetour(triangle.c);
        vertices.insert(vertices.end(), {a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2]});
        indices.insert(indices.end(), {nextIndex, nextIndex + 2, nextIndex + 1});
        areas.push_back(triangle.area);
        nextIndex += 3;
    }

    const int vertexCount = static_cast<int>(vertices.size() / 3u);
    const int triangleCount = static_cast<int>(indices.size() / 3u);

    rcConfig cfg{};
    cfg.cs = settings_.cellSize;
    cfg.ch = settings_.cellHeight;
    cfg.walkableSlopeAngle = agent_.maxSlopeAngleDegrees;
    cfg.walkableHeight = static_cast<int>(std::ceil(agent_.height() / cfg.ch));
    cfg.walkableClimb = static_cast<int>(std::floor(agent_.maxStepHeight / cfg.ch));
    cfg.walkableRadius = static_cast<int>(std::ceil(agent_.radius / cfg.cs));
    cfg.maxEdgeLen = static_cast<int>(settings_.edgeMaxLen / cfg.cs);
    cfg.maxSimplificationError = settings_.edgeMaxError;
    cfg.minRegionArea = static_cast<int>(settings_.regionMinSize * settings_.regionMinSize);
    cfg.mergeRegionArea = static_cast<int>(settings_.regionMergeSize * settings_.regionMergeSize);
    cfg.maxVertsPerPoly = 6;
    cfg.detailSampleDist = settings_.detailSampleDist < 0.9f
        ? 0.f
        : cfg.cs * settings_.detailSampleDist;
    cfg.detailSampleMaxError = cfg.ch * settings_.detailSampleMaxError;
    copyBounds(bounds_, cfg.bmin, cfg.bmax);
    const float horizontalPadding = cfg.cs * 2.f;
    cfg.bmin[0] -= horizontalPadding;
    cfg.bmin[2] -= horizontalPadding;
    cfg.bmax[0] += horizontalPadding;
    cfg.bmax[2] += horizontalPadding;
    cfg.bmin[1] -= std::max(agent_.maxStepHeight, cfg.ch * 2.f);
    cfg.bmax[1] += agent_.height();
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

    if (cfg.width <= 0 || cfg.height <= 0) {
        error = "nav geometry has invalid Recast raster size";
        return false;
    }

    std::unique_ptr<rcHeightfield, HeightFieldDeleter> heightfield(rcAllocHeightfield());
    if (!heightfield) {
        error = "cannot allocate Recast heightfield";
        return false;
    }
    if (!rcCreateHeightfield(&ctx, *heightfield, cfg.width, cfg.height, cfg.bmin, cfg.bmax,
                             cfg.cs, cfg.ch)) {
        error = "cannot create Recast heightfield";
        return false;
    }

    rcClearUnwalkableTriangles(&ctx, cfg.walkableSlopeAngle, vertices.data(), vertexCount,
                               indices.data(), triangleCount, areas.data());
    if (!rcRasterizeTriangles(&ctx, vertices.data(), vertexCount, indices.data(), areas.data(),
                              triangleCount, *heightfield, cfg.walkableClimb)) {
        error = "cannot rasterize nav geometry";
        return false;
    }

    rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *heightfield);
    rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *heightfield);
    rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *heightfield);

    std::unique_ptr<rcCompactHeightfield, CompactHeightFieldDeleter> compact(
        rcAllocCompactHeightfield());
    if (!compact) {
        error = "cannot allocate compact Recast heightfield";
        return false;
    }
    if (!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *heightfield,
                                   *compact)) {
        error = "cannot build compact Recast heightfield";
        return false;
    }
    if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *compact)) {
        error = "cannot erode Recast walkable area";
        return false;
    }
    if (!rcBuildDistanceField(&ctx, *compact)) {
        error = "cannot build Recast distance field";
        return false;
    }
    if (!rcBuildRegions(&ctx, *compact, 0, cfg.minRegionArea, cfg.mergeRegionArea)) {
        error = "cannot build Recast regions";
        return false;
    }

    std::unique_ptr<rcContourSet, ContourSetDeleter> contours(rcAllocContourSet());
    if (!contours) {
        error = "cannot allocate Recast contours";
        return false;
    }
    if (!rcBuildContours(&ctx, *compact, cfg.maxSimplificationError, cfg.maxEdgeLen, *contours)) {
        error = "cannot build Recast contours";
        return false;
    }

    std::unique_ptr<rcPolyMesh, PolyMeshDeleter> polyMesh(rcAllocPolyMesh());
    if (!polyMesh) {
        error = "cannot allocate Recast poly mesh";
        return false;
    }
    if (!rcBuildPolyMesh(&ctx, *contours, cfg.maxVertsPerPoly, *polyMesh)) {
        error = "cannot build Recast poly mesh";
        return false;
    }

    std::unique_ptr<rcPolyMeshDetail, PolyMeshDetailDeleter> detailMesh(rcAllocPolyMeshDetail());
    if (!detailMesh) {
        error = "cannot allocate Recast detail mesh";
        return false;
    }
    if (!rcBuildPolyMeshDetail(&ctx, *polyMesh, *compact, cfg.detailSampleDist,
                               cfg.detailSampleMaxError, *detailMesh)) {
        error = "cannot build Recast detail mesh";
        return false;
    }

    walkablePolyCount_ = 0;
    for (int i = 0; i < polyMesh->npolys; ++i) {
        if (polyMesh->areas[i] == kGroundArea) {
            polyMesh->flags[i] = kWalkFlag;
            ++walkablePolyCount_;
        } else {
            polyMesh->flags[i] = 0;
        }
    }
    if (walkablePolyCount_ == 0) {
        error = "Recast generated no walkable polygons";
        return false;
    }

    dtNavMeshCreateParams params{};
    params.verts = polyMesh->verts;
    params.vertCount = polyMesh->nverts;
    params.polys = polyMesh->polys;
    params.polyAreas = polyMesh->areas;
    params.polyFlags = polyMesh->flags;
    params.polyCount = polyMesh->npolys;
    params.nvp = polyMesh->nvp;
    params.detailMeshes = detailMesh->meshes;
    params.detailVerts = detailMesh->verts;
    params.detailVertsCount = detailMesh->nverts;
    params.detailTris = detailMesh->tris;
    params.detailTriCount = detailMesh->ntris;
    params.walkableHeight = agent_.height();
    params.walkableRadius = agent_.radius;
    params.walkableClimb = agent_.maxStepHeight;
    rcVcopy(params.bmin, polyMesh->bmin);
    rcVcopy(params.bmax, polyMesh->bmax);
    params.cs = cfg.cs;
    params.ch = cfg.ch;
    params.buildBvTree = true;

    unsigned char* navData = nullptr;
    int navDataSize = 0;
    if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
        error = "cannot create Detour navmesh data";
        return false;
    }

    std::unique_ptr<dtNavMesh, NavMeshDeleter> newNavMesh(dtAllocNavMesh());
    if (!newNavMesh) {
        dtFree(navData);
        error = "cannot allocate Detour navmesh";
        return false;
    }
    if (dtStatusFailed(newNavMesh->init(navData, navDataSize, DT_TILE_FREE_DATA))) {
        dtFree(navData);
        error = "cannot initialize Detour navmesh";
        return false;
    }

    std::unique_ptr<dtNavMeshQuery, NavMeshQueryDeleter> newQuery(dtAllocNavMeshQuery());
    if (!newQuery) {
        error = "cannot allocate Detour navmesh query";
        return false;
    }
    if (dtStatusFailed(newQuery->init(newNavMesh.get(), 2048))) {
        error = "cannot initialize Detour navmesh query";
        return false;
    }

    navMesh_ = std::move(newNavMesh);
    query_ = std::move(newQuery);
    return true;
}

std::array<float, 3> Map::queryExtents(const nav::Agent& agent) const {
    const float horizontal = std::max({agent.radius * 2.f, settings_.cellSize * 2.f, 100.f});
    float vertical = std::max(agent.height(), 300.f);
    if (bounds_.valid) {
        vertical = std::max(vertical, (bounds_.max.z - bounds_.min.z) * 0.5f + agent.height());
    }
    return {horizontal, vertical, horizontal};
}

std::array<float, 3> Map::toDetour(const nav::Vec3& value) {
    return {value.x, value.z, value.y};
}

nav::Vec3 Map::fromDetour(const float* value) {
    return nav::Vec3{value[0], value[2], value[1]};
}

bool Map::findNearestPoly(float x, float y, const nav::Agent& agent, float horizontalRadius,
                          nav::Vec3& outLocation) const {
    if (!hasNavMesh() || filter_ == nullptr) {
        return false;
    }

    const float centerZ = bounds_.valid ? (bounds_.min.z + bounds_.max.z) * 0.5f : 0.f;
    const nav::Vec3 serverCenter{x, y, centerZ};
    const std::array<float, 3> center = toDetour(serverCenter);
    std::array<float, 3> extents = queryExtents(agent);
    extents[0] = std::max(extents[0], horizontalRadius);
    extents[2] = std::max(extents[2], horizontalRadius);

    dtPolyRef ref = 0;
    float nearest[3]{};
    if (dtStatusFailed(query_->findNearestPoly(center.data(), extents.data(), filter_.get(), &ref,
                                               nearest)) ||
        ref == 0) {
        return false;
    }

    const nav::Vec3 converted = fromDetour(nearest);
    const float horizontalDistance =
        std::sqrt(sqr(converted.x - x) + sqr(converted.y - y));
    if (horizontalDistance > horizontalRadius) {
        return false;
    }

    outLocation = converted;
    return true;
}

bool Map::canStandAt(float x, float y, const nav::Agent& agent,
                     nav::Vec3* groundedLocation) const {
    nav::Vec3 location;
    if (!findNearestPoly(x, y, agent, std::max(agent.radius, settings_.cellSize), location)) {
        return false;
    }
    if (groundedLocation != nullptr) {
        *groundedLocation = location;
    }
    return true;
}

bool Map::blockedAlong(const nav::Vec3& from, const nav::Vec3& to,
                       const nav::Agent& agent) const {
    if (!hasNavMesh() || filter_ == nullptr) {
        return false;
    }

    nav::Vec3 groundedFrom;
    nav::Vec3 groundedTo;
    if (!findNearestPoly(from.x, from.y, agent, std::max(agent.radius, settings_.cellSize),
                         groundedFrom) ||
        !findNearestPoly(to.x, to.y, agent, std::max(agent.radius, settings_.cellSize),
                         groundedTo)) {
        return true;
    }

    const std::array<float, 3> start = toDetour(groundedFrom);
    const std::array<float, 3> end = toDetour(groundedTo);
    const std::array<float, 3> extents = queryExtents(agent);

    dtPolyRef startRef = 0;
    float nearestStart[3]{};
    if (dtStatusFailed(query_->findNearestPoly(start.data(), extents.data(), filter_.get(),
                                               &startRef, nearestStart)) ||
        startRef == 0) {
        return true;
    }

    float t = 0.f;
    float normal[3]{};
    dtPolyRef visited[kMaxRaycastPolys]{};
    int visitedCount = 0;
    const dtStatus rayStatus = query_->raycast(startRef, nearestStart, end.data(), filter_.get(),
                                               &t, normal, visited, &visitedCount,
                                               kMaxRaycastPolys);
    if (dtStatusFailed(rayStatus)) {
        return true;
    }

    return t < 1.f;
}

bool Map::nearestStandable(float x, float y, float radius, const nav::Agent& agent,
                           nav::Vec3& outLocation) const {
    if (!hasNavMesh() || filter_ == nullptr) {
        return false;
    }

    if (findNearestPoly(x, y, agent, std::max(radius, agent.radius), outLocation)) {
        return true;
    }

    const float centerZ = bounds_.valid ? (bounds_.min.z + bounds_.max.z) * 0.5f : 0.f;
    const std::array<float, 3> center = toDetour(nav::Vec3{x, y, centerZ});
    std::array<float, 3> extents = queryExtents(agent);
    extents[0] = std::max(extents[0], radius);
    extents[2] = std::max(extents[2], radius);

    dtPolyRef refs[kMaxNearestPolys]{};
    int count = 0;
    const dtStatus status = query_->queryPolygons(center.data(), extents.data(), filter_.get(),
                                                  refs, &count, kMaxNearestPolys);
    if (dtStatusFailed(status) || count <= 0) {
        return false;
    }

    float bestDistanceSquared = std::numeric_limits<float>::max();
    bool found = false;
    for (int i = 0; i < count; ++i) {
        float closest[3]{};
        bool posOverPoly = false;
        if (dtStatusFailed(query_->closestPointOnPoly(refs[i], center.data(), closest,
                                                      &posOverPoly))) {
            continue;
        }
        const nav::Vec3 candidate = fromDetour(closest);
        const float dx = candidate.x - x;
        const float dy = candidate.y - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 <= radius * radius && d2 < bestDistanceSquared) {
            bestDistanceSquared = d2;
            outLocation = candidate;
            found = true;
        }
    }
    return found;
}

}  // namespace heaven::field
