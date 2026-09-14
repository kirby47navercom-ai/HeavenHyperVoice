#include "NavigationMesh.h"

#include <DetourAlloc.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <Recast.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace heaven {
namespace {

using hhv::movement::Vec3;
constexpr float CellSize = 20.f;
constexpr float CellHeight = 2.f;
constexpr int TileCells = 256;
constexpr unsigned short WalkFlag = 1;

// 축 교환으로 좌우 방향이 반전되므로 삼각형의 꼭짓점 순서도 함께 뒤집는다.
std::array<float, 3> toNavigation(Vec3 point) {
    return {point.x, point.z, point.y};
}

struct MeshDeleter {
    void operator()(dtNavMesh *mesh) const {
        dtFreeNavMesh(mesh);
    }
};

using Mesh = std::unique_ptr<dtNavMesh, MeshDeleter>;

bool buildTile(const std::vector<hhv::movement::Triangle> &triangles,
               const std::vector<std::size_t> &selected, rcConfig config, const hhv::movement::Config &agent,
               int tileX, int tileY, dtNavMesh &mesh, std::size_t &polygons, std::string &error) {
    rcContext context(false);
    std::vector<float> vertices;
    std::vector<int> indices;
    vertices.reserve(selected.size() * 9);
    indices.reserve(selected.size() * 3);
    for (const auto index : selected) {
        const auto &triangle = triangles[index];
        const int base = static_cast<int>(vertices.size() / 3);
        for (const auto point : {triangle.a, triangle.c, triangle.b}) {
            const auto converted = toNavigation(point);
            vertices.insert(vertices.end(), converted.begin(), converted.end());
        }
        indices.insert(indices.end(), {base, base + 1, base + 2});
    }
    const int vertexCount = static_cast<int>(vertices.size() / 3);
    const int triangleCount = static_cast<int>(selected.size());
    std::vector<unsigned char> areas(selected.size(), RC_NULL_AREA);
    rcMarkWalkableTriangles(&context, config.walkableSlopeAngle, vertices.data(), vertexCount, indices.data(),
                            triangleCount, areas.data());

    const auto fail = [&](const char *stage) {
        error = "NavMesh tile (" + std::to_string(tileX) + "," + std::to_string(tileY) + "): " + stage;
        return false;
    };
    std::unique_ptr<rcHeightfield, decltype(&rcFreeHeightField)> heightfield(rcAllocHeightfield(),
                                                                             rcFreeHeightField);
    if (!heightfield || !rcCreateHeightfield(&context, *heightfield, config.width, config.height, config.bmin,
                                             config.bmax, config.cs, config.ch)) {
        return fail("heightfield allocation failed");
    }
    if (!rcRasterizeTriangles(&context, vertices.data(), vertexCount, indices.data(), areas.data(),
                              triangleCount, *heightfield, config.walkableClimb)) {
        return fail("rasterization failed");
    }
    rcFilterLowHangingWalkableObstacles(&context, config.walkableClimb, *heightfield);
    rcFilterLedgeSpans(&context, config.walkableHeight, config.walkableClimb, *heightfield);
    rcFilterWalkableLowHeightSpans(&context, config.walkableHeight, *heightfield);

    std::unique_ptr<rcCompactHeightfield, decltype(&rcFreeCompactHeightfield)> compact(
        rcAllocCompactHeightfield(), rcFreeCompactHeightfield);
    if (!compact || !rcBuildCompactHeightfield(&context, config.walkableHeight, config.walkableClimb,
                                               *heightfield, *compact)) {
        return fail("compact heightfield failed");
    }
    heightfield.reset();
    if (compact->spanCount == 0) {
        return true;
    }
    if (!rcErodeWalkableArea(&context, config.walkableRadius, *compact) ||
        !rcBuildDistanceField(&context, *compact) ||
        !rcBuildRegions(&context, *compact, config.borderSize, config.minRegionArea,
                        config.mergeRegionArea)) {
        return fail("walkable region generation failed");
    }

    std::unique_ptr<rcContourSet, decltype(&rcFreeContourSet)> contours(rcAllocContourSet(),
                                                                        rcFreeContourSet);
    if (!contours ||
        !rcBuildContours(&context, *compact, config.maxSimplificationError, config.maxEdgeLen, *contours)) {
        return fail("contour generation failed");
    }
    if (contours->nconts == 0) {
        return true;
    }
    std::unique_ptr<rcPolyMesh, decltype(&rcFreePolyMesh)> polyMesh(rcAllocPolyMesh(), rcFreePolyMesh);
    if (!polyMesh || !rcBuildPolyMesh(&context, *contours, config.maxVertsPerPoly, *polyMesh)) {
        return fail("polygon generation failed");
    }
    if (polyMesh->npolys == 0) {
        return true;
    }
    if (polyMesh->npolys > mesh.getParams()->maxPolys) {
        return fail("polygon limit exceeded");
    }
    std::unique_ptr<rcPolyMeshDetail, decltype(&rcFreePolyMeshDetail)> detail(rcAllocPolyMeshDetail(),
                                                                              rcFreePolyMeshDetail);
    if (!detail || !rcBuildPolyMeshDetail(&context, *polyMesh, *compact, config.detailSampleDist,
                                          config.detailSampleMaxError, *detail)) {
        return fail("detail mesh generation failed");
    }
    for (int index = 0; index < polyMesh->npolys; ++index) {
        polyMesh->flags[index] = WalkFlag;
    }

    dtNavMeshCreateParams params{};
    params.verts = polyMesh->verts;
    params.vertCount = polyMesh->nverts;
    params.polys = polyMesh->polys;
    params.polyAreas = polyMesh->areas;
    params.polyFlags = polyMesh->flags;
    params.polyCount = polyMesh->npolys;
    params.nvp = polyMesh->nvp;
    params.detailMeshes = detail->meshes;
    params.detailVerts = detail->verts;
    params.detailVertsCount = detail->nverts;
    params.detailTris = detail->tris;
    params.detailTriCount = detail->ntris;
    params.walkableHeight = agent.halfHeight * 2.f;
    params.walkableRadius = agent.radius;
    params.walkableClimb = agent.stepHeight;
    params.tileX = tileX;
    params.tileY = tileY;
    rcVcopy(params.bmin, polyMesh->bmin);
    rcVcopy(params.bmax, polyMesh->bmax);
    params.cs = config.cs;
    params.ch = config.ch;
    params.buildBvTree = true;

    unsigned char *bytes = nullptr;
    int byteCount = 0;
    if (!dtCreateNavMeshData(&params, &bytes, &byteCount)) {
        return fail("Detour data creation failed");
    }
    if (dtStatusFailed(mesh.addTile(bytes, byteCount, DT_TILE_FREE_DATA, 0, nullptr))) {
        dtFree(bytes);
        return fail("Detour tile insertion failed");
    }
    polygons += polyMesh->npolys;
    return true;
}

} // namespace

struct NavigationMesh::Data {
    Mesh mesh;
    hhv::movement::Config agent;
    dtQueryFilter filter;
    std::size_t polygons = 0;

    Data() {
        filter.setIncludeFlags(WalkFlag);
        filter.setExcludeFlags(0);
    }

    bool locate(const dtNavMeshQuery &query, Vec3 center, dtPolyRef &ref, float *point) const {
        if (!hhv::movement::finite(center)) {
            return false;
        }
        center.z -= agent.halfHeight;
        const auto target = toNavigation(center);
        // 캡슐 중심과 NavMesh 표면을 구분한다. 높이 범위를 제한해 위층/아래층을 섞지 않는다.
        const float extents[] = {2.f, agent.stepHeight + CellHeight * 2.f, 2.f};
        ref = 0;
        return dtStatusSucceed(query.findNearestPoly(target.data(), extents, &filter, &ref, point)) &&
               ref != 0 && std::hypot(point[0] - target[0], point[2] - target[2]) <= 2.f &&
               std::abs(point[1] - target[1]) <= extents[1];
    }
};

NavigationMesh::NavigationMesh() = default;
NavigationMesh::~NavigationMesh() = default;

bool NavigationMesh::build(const hhv::movement::TriangleWorld &collision, const hhv::movement::Config &agent,
                           std::string &error) {
    data_.reset();
    error.clear();
    const auto &triangles = collision.triangles();
    if (triangles.empty()) {
        error = "NavMesh requires exported collision triangles";
        return false;
    }
    if (!std::isfinite(agent.radius) || !std::isfinite(agent.halfHeight) ||
        !std::isfinite(agent.stepHeight) || !std::isfinite(agent.slopeDegrees) || agent.radius <= 0.f ||
        agent.halfHeight < agent.radius || agent.stepHeight < 0.f || agent.slopeDegrees <= 0.f ||
        agent.slopeDegrees >= 90.f || agent.radius > 1000.f || agent.halfHeight > 1000.f ||
        agent.stepHeight > agent.halfHeight) {
        error = "Unsupported NavMesh agent dimensions or slope limit";
        return false;
    }
    rcConfig config{};
    config.cs = CellSize;
    config.ch = CellHeight;
    config.walkableSlopeAngle = agent.slopeDegrees;
    config.walkableHeight = static_cast<int>(std::ceil(agent.halfHeight * 2.f / CellHeight));
    config.walkableClimb = static_cast<int>(std::floor(agent.stepHeight / CellHeight));
    config.walkableRadius = static_cast<int>(std::ceil(agent.radius / CellSize));
    config.borderSize = config.walkableRadius + 3;
    config.tileSize = TileCells;
    config.width = config.height = TileCells + config.borderSize * 2;
    config.maxEdgeLen = 60;
    config.maxSimplificationError = 1.f;
    config.minRegionArea = 0;
    config.mergeRegionArea = 400;
    config.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
    config.detailSampleDist = CellSize * 6.f;
    config.detailSampleMaxError = CellHeight;

    std::array<float, 3> minimum{INFINITY, INFINITY, INFINITY};
    std::array<float, 3> maximum{-INFINITY, -INFINITY, -INFINITY};
    for (const auto &triangle : triangles) {
        for (const auto vertex : {triangle.a, triangle.b, triangle.c}) {
            const auto point = toNavigation(vertex);
            for (int axis = 0; axis < 3; ++axis) {
                minimum[axis] = std::min(minimum[axis], point[axis]);
                maximum[axis] = std::max(maximum[axis], point[axis]);
            }
        }
    }
    minimum[1] -= agent.stepHeight;
    maximum[1] += agent.halfHeight * 2.f;
    constexpr float TileWidth = TileCells * CellSize;
    const float width = maximum[0] - minimum[0];
    const float depth = maximum[2] - minimum[2];
    if (!std::isfinite(width) || !std::isfinite(depth) || width > 4096 * TileWidth ||
        depth > 4096 * TileWidth) {
        error = "Collision bounds exceed the supported NavMesh extent";
        return false;
    }
    const int columns = std::max(1, static_cast<int>(std::ceil(width / TileWidth)));
    const int rows = std::max(1, static_cast<int>(std::ceil(depth / TileWidth)));
    if (columns > 4096 || rows > 4096 || columns * rows > 16384) {
        error = "Collision bounds exceed the supported NavMesh tile/height limits";
        return false;
    }

    auto next = std::make_unique<Data>();
    next->agent = agent;
    next->mesh.reset(dtAllocNavMesh());
    int tileBits = 0;
    while ((1 << tileBits) < columns * rows) {
        ++tileBits;
    }
    dtNavMeshParams params{};
    rcVcopy(params.orig, minimum.data());
    params.tileWidth = params.tileHeight = TileWidth;
    params.maxTiles = 1 << tileBits;
    params.maxPolys = 1 << (22 - tileBits);
    if (!next->mesh || dtStatusFailed(next->mesh->init(&params))) {
        error = "Cannot initialize tiled Detour NavMesh";
        return false;
    }

    // 삼각형을 한 번만 분류한다. 타일마다 전체 Filed 지형을 다시 훑지 않는다.
    std::vector<std::vector<std::size_t>> buckets(columns * rows);
    const float border = config.borderSize * CellSize;
    for (std::size_t index = 0; index < triangles.size(); ++index) {
        const auto &t = triangles[index];
        const auto tileIndex = [](float value, float origin, int count) {
            return std::clamp(static_cast<int>(std::floor((value - origin) / TileWidth)), 0, count - 1);
        };
        const int x0 = tileIndex(std::min({t.a.x, t.b.x, t.c.x}) - border, minimum[0], columns);
        const int x1 = tileIndex(std::max({t.a.x, t.b.x, t.c.x}) + border, minimum[0], columns);
        const int y0 = tileIndex(std::min({t.a.y, t.b.y, t.c.y}) - border, minimum[2], rows);
        const int y1 = tileIndex(std::max({t.a.y, t.b.y, t.c.y}) + border, minimum[2], rows);
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                buckets[y * columns + x].push_back(index);
            }
        }
    }
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            const auto &selected = buckets[y * columns + x];
            if (selected.empty()) {
                continue;
            }
            rcVcopy(config.bmin, minimum.data());
            rcVcopy(config.bmax, maximum.data());
            config.bmin[0] = minimum[0] + x * TileWidth - border;
            config.bmin[2] = minimum[2] + y * TileWidth - border;
            config.bmax[0] = minimum[0] + (x + 1) * TileWidth + border;
            config.bmax[2] = minimum[2] + (y + 1) * TileWidth + border;
            config.bmin[1] = INFINITY;
            config.bmax[1] = -INFINITY;
            for (const auto index : selected) {
                const auto &t = triangles[index];
                config.bmin[1] = std::min({config.bmin[1], t.a.z, t.b.z, t.c.z});
                config.bmax[1] = std::max({config.bmax[1], t.a.z, t.b.z, t.c.z});
            }
            config.bmin[1] = std::floor((config.bmin[1] - agent.stepHeight) / CellHeight) * CellHeight;
            config.bmax[1] += agent.halfHeight * 2.f;
            if ((config.bmax[1] - config.bmin[1]) / CellHeight >= RC_SPAN_MAX_HEIGHT) {
                error = "NavMesh tile exceeds Recast vertical span limits";
                return false;
            }
            if (!buildTile(triangles, selected, config, agent, x, y, *next->mesh, next->polygons, error)) {
                return false;
            }
        }
    }
    if (next->polygons == 0) {
        error = "Recast generated no walkable polygons from the exported collision";
        return false;
    }
    data_ = std::move(next);
    return true;
}

bool NavigationMesh::findPath(Vec3 start, Vec3 goal, int maxNodes, std::vector<Vec3> &points) const {
    points.clear();
    if (!data_) {
        return false;
    }
    dtNavMeshQuery query;
    const int capacity = std::clamp(maxNodes, 64, 8192);
    if (dtStatusFailed(query.init(data_->mesh.get(), capacity))) {
        return false;
    }
    dtPolyRef startRef = 0, goalRef = 0;
    float from[3], to[3];
    if (!data_->locate(query, start, startRef, from) || !data_->locate(query, goal, goalRef, to)) {
        return false;
    }
    std::vector<dtPolyRef> corridor(capacity);
    int count = 0;
    const auto status =
        query.findPath(startRef, goalRef, from, to, &data_->filter, corridor.data(), &count, capacity);
    if (dtStatusFailed(status) || dtStatusDetail(status, DT_BUFFER_TOO_SMALL) ||
        dtStatusDetail(status, DT_OUT_OF_NODES) || dtStatusDetail(status, DT_PARTIAL_RESULT) || count == 0 ||
        corridor[count - 1] != goalRef) {
        return false;
    }
    std::vector<float> straight(capacity * 3);
    int pointCount = 0;
    const auto straightStatus = query.findStraightPath(from, to, corridor.data(), count, straight.data(),
                                                       nullptr, nullptr, &pointCount, capacity);
    if (dtStatusFailed(straightStatus) || dtStatusDetail(straightStatus, DT_BUFFER_TOO_SMALL)) {
        return false;
    }
    for (int index = 0; index < pointCount; ++index) {
        points.push_back({straight[index * 3], straight[index * 3 + 2],
                          straight[index * 3 + 1] + data_->agent.halfHeight});
    }

    // 최단 경로의 꺾임점은 장애물 경계에 붙는다. 관성으로 경계를 스치지 않도록
    // 여유가 있는 모서리만 안쪽으로 조금 옮기고, 양쪽 연결을 NavMesh로 다시 확인한다.
    for (std::size_t index = 1; index + 1 < points.size(); ++index) {
        const auto corner = points[index];
        const auto before = points[index - 1] - corner;
        const auto after = points[index + 1] - corner;
        const float beforeLength = std::hypot(before.x, before.y);
        const float afterLength = std::hypot(after.x, after.y);
        if (beforeLength < 1.f || afterLength < 1.f) {
            continue;
        }
        Vec3 away{-before.x / beforeLength - after.x / afterLength,
                  -before.y / beforeLength - after.y / afterLength, 0.f};
        const float length = std::hypot(away.x, away.y);
        if (length < .01f) {
            continue;
        }
        const float inset = std::min({10.f, beforeLength * .1f, afterLength * .1f});
        const auto candidate = corner + away * (inset / length);
        dtPolyRef ref = 0;
        float projected[3];
        if (!data_->locate(query, candidate, ref, projected)) {
            continue;
        }
        const Vec3 adjusted{projected[0], projected[2], projected[1] + data_->agent.halfHeight};
        if (clearSegment(points[index - 1], adjusted) && clearSegment(adjusted, points[index + 1])) {
            points[index] = adjusted;
        }
    }
    return !points.empty();
}

bool NavigationMesh::clearSegment(Vec3 start, Vec3 goal) const {
    if (!data_) {
        return false;
    }
    dtNavMeshQuery query;
    if (dtStatusFailed(query.init(data_->mesh.get(), 64))) {
        return false;
    }
    dtPolyRef startRef = 0, goalRef = 0;
    float from[3], to[3];
    if (!data_->locate(query, start, startRef, from) || !data_->locate(query, goal, goalRef, to)) {
        return false;
    }
    std::array<dtPolyRef, 512> visited;
    int count = 0;
    float fraction = 0;
    float normal[3];
    const auto status = query.raycast(startRef, from, to, &data_->filter, &fraction, normal, visited.data(),
                                      &count, static_cast<int>(visited.size()));
    if (dtStatusFailed(status) || dtStatusDetail(status, DT_BUFFER_TOO_SMALL) || fraction < 1.f ||
        count == 0) {
        return false;
    }
    // 공유 모서리에서는 nearestPoly가 이웃 폴리곤을 고를 수 있다. 번호 대신 실제 층을 비교한다.
    // Detour raycast는 수평 검사이므로 높이 확인을 생략하면 위층/아래층을 통과시킬 수 있다.
    float reached[3];
    return dtStatusSucceed(query.closestPointOnPoly(visited[count - 1], to, reached, nullptr)) &&
           std::hypot(reached[0] - to[0], reached[2] - to[2]) <= .1f &&
           std::abs(reached[1] - to[1]) <= CellHeight * 2.f;
}

std::size_t NavigationMesh::polygonCount() const {
    return data_ ? data_->polygons : 0;
}

} // namespace heaven
