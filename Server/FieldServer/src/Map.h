#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class dtNavMesh;
class dtNavMeshQuery;
class dtQueryFilter;

namespace heaven::field {

namespace nav {

struct Vec3 {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

struct Aabb {
    Vec3 min;
    Vec3 max;
    bool valid = false;
};

struct Agent {
    float radius = 34.f;
    float halfHeight = 88.f;
    float maxStepHeight = 45.f;
    float maxSlopeAngleDegrees = 44.f;
    int maxSearchNodes = 4096;

    float height() const { return halfHeight * 2.f; }
};

struct BuildSettings {
    float cellSize = 50.f;
    float cellHeight = 5.f;
    float regionMinSize = 8.f;
    float regionMergeSize = 20.f;
    float edgeMaxLen = 1200.f;
    float edgeMaxError = 1.3f;
    float detailSampleDist = 6.f;
    float detailSampleMaxError = 1.f;
};

struct Triangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;
    unsigned char area = 1;
};

}  // namespace nav

class Map {
public:
    Map();
    ~Map();

    Map(const Map&) = delete;
    Map& operator=(const Map&) = delete;
    Map(Map&&) noexcept;
    Map& operator=(Map&&) noexcept;

    bool loadFromFile(const std::string& path, std::string& error);

    bool loaded() const { return loaded_; }
    bool hasNavMesh() const { return navMesh_ != nullptr && query_ != nullptr; }

    std::size_t groundCount() const { return groundTriangleCount_; }
    std::size_t wallCount() const { return wallTriangleCount_; }
    std::size_t triangleCount() const { return triangles_.size(); }
    std::size_t walkablePolyCount() const { return walkablePolyCount_; }

    const nav::Aabb& bounds() const { return bounds_; }
    const nav::Agent& agent() const { return agent_; }
    const nav::BuildSettings& settings() const { return settings_; }

    bool canStandAt(float x, float y, const nav::Agent& agent,
                    nav::Vec3* groundedLocation = nullptr) const;
    bool blockedAlong(const nav::Vec3& from, const nav::Vec3& to,
                      const nav::Agent& agent) const;
    bool nearestStandable(float x, float y, float radius, const nav::Agent& agent,
                          nav::Vec3& outLocation) const;

    const dtNavMeshQuery* navQuery() const { return query_.get(); }
    const dtQueryFilter* queryFilter() const { return filter_.get(); }
    std::array<float, 3> queryExtents(const nav::Agent& agent) const;

    static std::array<float, 3> toDetour(const nav::Vec3& value);
    static nav::Vec3 fromDetour(const float* value);

private:
    struct NavMeshDeleter {
        void operator()(dtNavMesh* mesh) const;
    };

    struct NavMeshQueryDeleter {
        void operator()(dtNavMeshQuery* query) const;
    };

    struct QueryFilterDeleter {
        void operator()(dtQueryFilter* filter) const;
    };

    bool parseLine(const std::string& line, int lineNumber, std::string& error);
    bool buildNavMesh(std::string& error);
    void clear();
    void updateBounds(const nav::Vec3& value);
    bool findNearestPoly(float x, float y, const nav::Agent& agent, float horizontalRadius,
                         nav::Vec3& outLocation) const;

    std::string source_;
    nav::Aabb bounds_;
    nav::Agent agent_;
    nav::BuildSettings settings_;
    std::vector<nav::Triangle> triangles_;
    std::size_t groundTriangleCount_ = 0;
    std::size_t wallTriangleCount_ = 0;
    std::size_t walkablePolyCount_ = 0;
    bool loaded_ = false;

    std::unique_ptr<dtNavMesh, NavMeshDeleter> navMesh_;
    std::unique_ptr<dtNavMeshQuery, NavMeshQueryDeleter> query_;
    std::unique_ptr<dtQueryFilter, QueryFilterDeleter> filter_;
};

}  // namespace heaven::field
