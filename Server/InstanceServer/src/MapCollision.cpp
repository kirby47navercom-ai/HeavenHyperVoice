#include "MapCollision.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace heaven::instance {

namespace {

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 subtract(const Vec3& a, const Vec3& b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 normalizeOr(const Vec3& v, const Vec3& fallback) {
    const float length = std::sqrt(dot(v, v));
    return length <= 1e-4f ? fallback : Vec3{v.x / length, v.y / length, v.z / length};
}

// 캡슐이 이 축 방향으로 얼마나 뻗어 있는가.
//
// 정확한 캡슐-OBB 거리 대신 축별 지지폭으로 OBB 를 부풀리고 점 포함으로 본다.
// 모서리에서 실제보다 조금 넓게 막지만, 판정이 보수적인 쪽으로 틀리는 것이
// 벽을 뚫는 것보다 낫다. 클라이언트도 같은 근사를 쓴다.
float capsuleSupport(const Vec3& axis, const AgentSettings& agent) {
    const float vertical = std::abs(axis.z) * agent.capsuleHalfHeight;
    const float horizontal = std::sqrt(std::max(0.f, 1.f - axis.z * axis.z)) * agent.capsuleRadius;
    return vertical + horizontal;
}

// 접촉 여유. 클라이언트 물리는 캡슐을 벽면에 정확히 붙여 세우는데, 그 위치를
// 그대로 차단하면 벽을 따라 걷는 것이 전부 거부되고 보정이 계속 날아간다.
// 딱 붙은 상태는 통과시키고 실제로 파고든 것만 막는다.
constexpr float kContactSkin = 2.f;

}  // namespace

// ------------------------------------------------------------------ Heightmap

bool Heightmap::sample(float x, float y, float& outZ) const {
    if (!valid()) {
        return false;
    }

    const float gx = (x - originX) / cellSize;
    const float gy = (y - originY) / cellSize;

    // NaN 은 모든 비교가 거짓이라 `< 0` 으로 쓰면 그대로 빠져나간다.
    // 통과 조건을 뒤집어 써서 NaN 도 여기서 걸리게 한다.
    if (!(gx >= 0.f) || !(gy >= 0.f) || !(gx <= static_cast<float>(cols - 1)) ||
        !(gy <= static_cast<float>(rows - 1))) {
        return false;
    }

    const int i0 = static_cast<int>(gx);
    const int j0 = static_cast<int>(gy);
    const int i1 = std::min(i0 + 1, cols - 1);
    const int j1 = std::min(j0 + 1, rows - 1);
    const float tx = gx - static_cast<float>(i0);
    const float ty = gy - static_cast<float>(j0);

    const auto at = [this](int i, int j) {
        return heights[static_cast<std::size_t>(j) * static_cast<std::size_t>(cols) +
                       static_cast<std::size_t>(i)];
    };

    const float top = at(i0, j0) + (at(i1, j0) - at(i0, j0)) * tx;
    const float bottom = at(i0, j1) + (at(i1, j1) - at(i0, j1)) * tx;
    outZ = top + (bottom - top) * ty;
    return true;
}

// --------------------------------------------------------------- MapCollision

bool MapCollision::loadFromFile(const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        error = "cannot open " + path;
        return false;
    }

    walls_.clear();
    floor_ = Heightmap{};
    bounds_ = BoundsSphere{};

    // 읽은 행에 표시를 남긴다. 빠진 행이 있으면 그 자리가 0 인 채로 통과해서
    // 지형에 구멍이 뚫린다 — 조용히 잘못되느니 로드를 실패시킨다.
    std::vector<bool> rowSeen;
    std::string line;
    int lineNumber = 0;

    const auto fail = [&](const std::string& what) {
        error = what + " at line " + std::to_string(lineNumber);
        walls_.clear();
        floor_ = Heightmap{};
        bounds_ = BoundsSphere{};
        return false;
    };

    while (std::getline(file, line)) {
        ++lineNumber;
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "wall_obb") {
            std::string profile;
            Obb wall;
            stream >> profile >> wall.center.x >> wall.center.y >> wall.center.z >>
                wall.halfExtent.x >> wall.halfExtent.y >> wall.halfExtent.z >> wall.axisX.x >>
                wall.axisX.y >> wall.axisX.z >> wall.axisY.x >> wall.axisY.y >> wall.axisY.z >>
                wall.axisZ.x >> wall.axisZ.y >> wall.axisZ.z;
            if (stream.fail()) {
                return fail("malformed wall_obb");
            }

            wall.axisX = normalizeOr(wall.axisX, Vec3{1.f, 0.f, 0.f});
            wall.axisY = normalizeOr(wall.axisY, Vec3{0.f, 1.f, 0.f});
            wall.axisZ = normalizeOr(wall.axisZ, Vec3{0.f, 0.f, 1.f});
            walls_.push_back(wall);
            continue;
        }

        if (type == "heightmap") {
            if (floor_.cols != 0) {
                return fail("duplicate heightmap");
            }
            stream >> floor_.originX >> floor_.originY >> floor_.cellSize >> floor_.cols >>
                floor_.rows;
            if (stream.fail()) {
                return fail("malformed heightmap");
            }
            if (floor_.cols <= 0 || floor_.rows <= 0 || !(floor_.cellSize > 0.f)) {
                return fail("heightmap needs positive cellSize, cols and rows");
            }
            // 1억 칸이면 400MB 다. 익스포터가 자릿수를 틀린 것을 여기서 잡는다.
            const std::uint64_t cells = static_cast<std::uint64_t>(floor_.cols) *
                                        static_cast<std::uint64_t>(floor_.rows);
            if (cells > 16u * 1024u * 1024u) {
                return fail("heightmap is too large (over 16M cells)");
            }
            floor_.heights.assign(static_cast<std::size_t>(cells), 0.f);
            rowSeen.assign(static_cast<std::size_t>(floor_.rows), false);
            continue;
        }

        if (type == "height_row") {
            if (floor_.cols == 0) {
                return fail("height_row before heightmap");
            }
            int row = -1;
            stream >> row;
            if (stream.fail() || row < 0 || row >= floor_.rows) {
                return fail("height_row index out of range");
            }
            for (int i = 0; i < floor_.cols; ++i) {
                float height = 0.f;
                stream >> height;
                if (stream.fail()) {
                    return fail("height_row has fewer values than cols");
                }
                floor_.heights[static_cast<std::size_t>(row) *
                                   static_cast<std::size_t>(floor_.cols) +
                               static_cast<std::size_t>(i)] = height;
            }
            rowSeen[static_cast<std::size_t>(row)] = true;
            continue;
        }

        if (type == "bounds_sphere") {
            stream >> bounds_.center.x >> bounds_.center.y >> bounds_.center.z >> bounds_.radius;
            if (stream.fail()) {
                return fail("malformed bounds_sphere");
            }
            if (!(bounds_.radius > 0.f)) {
                return fail("bounds_sphere needs a positive radius");
            }
            continue;
        }

        // 이 서버가 아직 안 보는 것들(ground_obb, spawn, nav...)은 넘긴다.
        // 형식을 공유하므로 같은 파일을 클라이언트도 그대로 읽는다.
    }

    if (floor_.cols != 0) {
        for (std::size_t row = 0; row < rowSeen.size(); ++row) {
            if (!rowSeen[row]) {
                error = "heightmap row " + std::to_string(row) + " is missing in " + path;
                walls_.clear();
                floor_ = Heightmap{};
                bounds_ = BoundsSphere{};
                return false;
            }
        }
    }

    if (!loaded()) {
        error = "no wall_obb, heightmap or bounds_sphere in " + path;
        return false;
    }
    return true;
}

std::string MapCollision::describe() const {
    std::string out = std::to_string(walls_.size()) + " walls";
    if (floor_.valid()) {
        out += ", heightmap " + std::to_string(floor_.cols) + "x" + std::to_string(floor_.rows) +
               " @" + std::to_string(static_cast<int>(floor_.cellSize)) + "uu";
    } else {
        out += ", no heightmap";
    }
    if (bounds_.active()) {
        out += ", bounds r=" + std::to_string(static_cast<int>(bounds_.radius));
    } else {
        out += ", no bounds";
    }
    return out;
}

bool MapCollision::floorAt(float x, float y, float& outZ) const {
    if (!floor_.valid()) {
        outZ = 0.f;  // 하이트맵이 없으면 평지로 본다
        return true;
    }
    return floor_.sample(x, y, outZ);
}

bool MapCollision::blockedByWall(const Vec3& capsuleCenter, const AgentSettings& agent) const {
    // ponytail: 벽 전체를 선형으로 훑는다. 20Hz x 접속자 x 벽 개수라 벽이 수천
    // 개가 되면 여기가 먼저 막힌다. 그때는 월드가 이미 쓰는 16x16 섹터 격자에
    // 벽을 버킷으로 나눠 담고 해당 섹터만 보면 된다.
    for (const Obb& wall : walls_) {
        const Vec3 delta = subtract(capsuleCenter, wall.center);

        const float limitX = wall.halfExtent.x + capsuleSupport(wall.axisX, agent) - kContactSkin;
        const float limitY = wall.halfExtent.y + capsuleSupport(wall.axisY, agent) - kContactSkin;
        const float limitZ = wall.halfExtent.z + capsuleSupport(wall.axisZ, agent) - kContactSkin;

        if (std::abs(dot(delta, wall.axisX)) >= limitX) {
            continue;
        }
        if (std::abs(dot(delta, wall.axisY)) >= limitY) {
            continue;
        }
        if (std::abs(dot(delta, wall.axisZ)) >= limitZ) {
            continue;
        }
        return true;
    }
    return false;
}

bool MapCollision::blockedAt(float x, float y, const AgentSettings& agent) const {
    float floorZ = 0.f;
    if (!floorAt(x, y, floorZ)) {
        return true;  // 지형 밖. 설 자리가 없다.
    }

    const Vec3 center{x, y, floorZ + agent.capsuleHalfHeight};
    if (bounds_.active()) {
        const Vec3 delta = subtract(center, bounds_.center);
        if (dot(delta, delta) > bounds_.radius * bounds_.radius) {
            return true;
        }
    }
    return blockedByWall(center, agent);
}

bool MapCollision::blockedAlong(float fromX, float fromY, float toX, float toY,
                                const AgentSettings& agent) const {
    const float dx = toX - fromX;
    const float dy = toY - fromY;
    const float distance = std::sqrt(dx * dx + dy * dy);

    // 캡슐 반지름마다 한 번. 이보다 성기면 그 사이로 얇은 벽이 지나갈 수 있다.
    const float step = std::max(1.f, agent.capsuleRadius);

    // 속도 상한이 이미 걸린 뒤라 거리가 폭주하지 않지만, 상한을 나중에 올리면
    // 여기가 조용히 비싸진다. 그래서 개수를 묶어둔다.
    constexpr int kMaxSamples = 64;

    // 개수를 묶은 채로 더 긴 구간을 받으면 샘플 간격이 캡슐 반지름보다 벌어져
    // 그 사이로 얇은 벽이 지나간다 — 검사가 켜진 채로 조용히 뚫리는 것이
    // 제일 나쁘다. 그럴 만큼 긴 이동은 통과시키지 않는다.
    if (distance > static_cast<float>(kMaxSamples) * step) {
        return true;
    }

    // 출발점의 바닥. 여기서부터 표본마다 높이차를 재서 절벽을 막는다.
    float previousZ = 0.f;
    if (!floorAt(fromX, fromY, previousZ)) {
        // 출발점이 지형 밖이다. 여기서 막으면 밖으로 밀려난 캐릭터가 영영
        // 갇히므로, 안으로 돌아오는 이동만 받아준다.
        previousZ = 0.f;
    }

    const int samples = std::min(kMaxSamples, static_cast<int>(distance / step) + 1);
    for (int i = 1; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float px = fromX + dx * t;
        const float py = fromY + dy * t;

        float floorZ = 0.f;
        if (!floorAt(px, py, floorZ)) {
            return true;  // 지형 밖으로 걸어 나가려 한다
        }
        if (floorZ - previousZ > agent.maxStepUp) {
            return true;  // 절벽을 수직으로 오르려 한다
        }
        previousZ = floorZ;

        const Vec3 center{px, py, floorZ + agent.capsuleHalfHeight};
        if (bounds_.active()) {
            const Vec3 delta = subtract(center, bounds_.center);
            if (dot(delta, delta) > bounds_.radius * bounds_.radius) {
                return true;  // 경계 구 밖
            }
        }
        if (blockedByWall(center, agent)) {
            return true;
        }
    }
    return false;
}

}  // namespace heaven::instance
