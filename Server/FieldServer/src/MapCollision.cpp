#include "MapCollision.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace heaven::field {

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

bool MapCollision::loadFromFile(const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        error = "cannot open " + path;
        return false;
    }

    walls_.clear();
    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream stream(line);
        std::string type;
        stream >> type;

        // 이 서버가 아직 안 보는 것들(heightmap, ground_obb, bounds...)은 넘긴다.
        // 형식을 공유하므로 같은 파일을 클라이언트도 그대로 읽는다.
        if (type != "wall_obb") {
            continue;
        }

        std::string profile;
        Obb wall;
        stream >> profile >> wall.center.x >> wall.center.y >> wall.center.z >>
            wall.halfExtent.x >> wall.halfExtent.y >> wall.halfExtent.z >> wall.axisX.x >>
            wall.axisX.y >> wall.axisX.z >> wall.axisY.x >> wall.axisY.y >> wall.axisY.z >>
            wall.axisZ.x >> wall.axisZ.y >> wall.axisZ.z;

        if (stream.fail()) {
            error = "malformed wall_obb at line " + std::to_string(lineNumber);
            walls_.clear();
            return false;
        }

        wall.axisX = normalizeOr(wall.axisX, Vec3{1.f, 0.f, 0.f});
        wall.axisY = normalizeOr(wall.axisY, Vec3{0.f, 1.f, 0.f});
        wall.axisZ = normalizeOr(wall.axisZ, Vec3{0.f, 0.f, 1.f});
        walls_.push_back(wall);
    }

    if (walls_.empty()) {
        error = "no wall_obb entries in " + path;
        return false;
    }
    return true;
}

bool MapCollision::blocked(const Vec3& capsuleCenter, const AgentSettings& agent) const {
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

bool MapCollision::blockedAlong(const Vec3& from, const Vec3& to,
                                const AgentSettings& agent) const {
    const Vec3 delta = subtract(to, from);
    const float distance = std::sqrt(dot(delta, delta));

    // 캡슐 반지름마다 한 번. 이보다 성기면 그 사이로 얇은 벽이 지나갈 수 있다.
    const float step = std::max(1.f, agent.capsuleRadius);

    // 속도 상한이 이미 걸린 뒤라 거리가 폭주하지 않지만, 상한을 나중에 올리면
    // 여기가 조용히 비싸진다. 그래서 개수를 묶어둔다.
    constexpr int kMaxSamples = 64;

    // 개수를 묶은 채로 더 긴 구간을 받으면 샘플 간격이 캡슐 반지름보다 벌어져
    // 그 사이로 얇은 벽이 지나간다 — 검사가 켜진 채로 조용히 뚫리는 것이
    // 제일 나쁘다. 그럴 만큼 긴 이동은 통과시키지 않는다.
    //
    // 정상 이동은 여기 닿지 않는다: kMaxSpeed x kMaxMoveElapsed + 여유 = 800uu 로
    // 24 샘플이면 되고, 야생은 한 틱에 그보다 훨씬 짧게 움직인다.
    if (distance > static_cast<float>(kMaxSamples) * step) {
        return true;
    }

    const int samples = std::min(kMaxSamples, static_cast<int>(distance / step) + 1);

    for (int i = 1; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const Vec3 point{from.x + delta.x * t, from.y + delta.y * t, from.z + delta.z * t};
        if (blocked(point, agent)) {
            return true;
        }
    }
    return false;
}

}  // namespace heaven::field
