#pragma once

// 인스턴스 방의 지형 판정. 벽 · 바닥 · 경계 셋을 본다.
//
// FieldServer/src/MapCollision.* 에서 갈라져 나온 사본이다. 필드 쪽은 벽만
// 보는 원래 판이고, 바닥과 경계는 여기서만 자란다 — 맵을 갖는 것은 인스턴스다.
//
//   wall_obb       상자 하나. 캡슐이 파고들면 막는다.
//   heightmap      바닥 높이 격자. 캡슐을 지형 위에 세우고, 격자 밖은 막는다.
//   bounds_sphere  놀 수 있는 구. 밖으로 나가려 하면 막는다.
//
// 같은 파일에 wild_species 줄도 있지만 그건 지형이 아니라서 여기서 안 읽는다
// (InstanceServer/src/main.cpp 의 readWildSpecies).
//
// 셋 다 없어도 된다. 없는 것은 검사하지 않는다 — 벽만 있는 맵, 바닥만 있는
// 맵 모두 유효하다. 다만 파일에 아무것도 없으면 로드 실패다 ("충돌 켜진 줄
// 알고 운영하는" 상태가 제일 나쁘다).
//
// 좌표는 전부 서버 좌표(uu)다. 언리얼 좌표에 WorldOriginOffset 을 더한 값이며
// 익스포터가 그 변환을 해서 내보낸다.

#include <cstdint>
#include <string>
#include <vector>

namespace heaven::instance {

struct Vec3 {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

// 축이 정규직교라고 가정한다. 맵 파일이 그렇게 내보낸다.
struct Obb {
    Vec3 center;
    Vec3 halfExtent;
    Vec3 axisX{1.f, 0.f, 0.f};
    Vec3 axisY{0.f, 1.f, 0.f};
    Vec3 axisZ{0.f, 0.f, 1.f};
};

// 캐릭터를 감싸는 캡슐. 클라이언트 기본값과 같아야 예측이 어긋나지 않는다.
struct AgentSettings {
    float capsuleRadius = 34.f;
    float capsuleHalfHeight = 88.f;

    // 표본 한 칸(= capsuleRadius) 사이에 오를 수 있는 높이.
    //
    // 이게 없으면 하이트맵이 있어도 절벽을 수직으로 걸어 올라간다. 34uu 를
    // 가면서 100uu 오르는 것까지 봐주므로 71도 경사까지 통과한다 —
    // 지형이 험하면 줄이고, 계단이 안 올라가지면 늘린다.
    float maxStepUp = 100.f;
};

// 바닥 높이 격자.
//
// (originX, originY) 는 [0][0] 칸 **중심**의 월드 좌표이고, 칸 간격은 cellSize
// 다. 격자 밖은 "바닥이 없다" 로 보고 이동을 막는다 — 지형 밖으로 걸어 나가는
// 것을 여기서 잡는다.
struct Heightmap {
    float originX = 0.f;
    float originY = 0.f;
    float cellSize = 0.f;
    int cols = 0;
    int rows = 0;
    std::vector<float> heights;  // rows * cols, 행 우선

    bool valid() const {
        return cols > 0 && rows > 0 && cellSize > 0.f &&
               heights.size() == static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
    }

    // 격자 밖이면 false. 안이면 이중선형 보간한 높이를 낸다.
    bool sample(float x, float y, float& outZ) const;
};

// 놀 수 있는 구. radius 가 0 이면 경계 검사를 하지 않는다.
struct BoundsSphere {
    Vec3 center;
    float radius = 0.f;

    bool active() const { return radius > 0.f; }
};

class MapCollision {
public:
    // 팀원 exporter 가 내보내는 텍스트 형식. 한 줄에 하나씩이고, 첫 토큰이
    // 아래 넷 중 하나가 아니면 그 줄은 넘긴다 (클라와 같은 파일을 쓸 수 있다).
    //
    //   wall_obb <profile> cx cy cz  hx hy hz  axX axY axZ  ayX ayY ayZ  azX azY azZ
    //   heightmap <originX> <originY> <cellSize> <cols> <rows>
    //   height_row <rowIndex> <h0> <h1> ... <h(cols-1)>
    //   bounds_sphere <cx> <cy> <cz> <radius>
    //
    // height_row 는 heightmap 줄 뒤에 와야 하고 rows 개가 다 있어야 한다.
    bool loadFromFile(const std::string& path, std::string& error);

    bool loaded() const { return !walls_.empty() || floor_.valid() || bounds_.active(); }
    std::size_t wallCount() const { return walls_.size(); }
    bool hasFloor() const { return floor_.valid(); }
    const BoundsSphere& bounds() const { return bounds_; }

    // 무엇을 들고 있는지 한 줄로. 기동 로그에 찍는다.
    std::string describe() const;

    // 바닥 높이. 하이트맵이 없으면 0 을 내고 true (평지로 본다).
    // 하이트맵이 있는데 격자 밖이면 false — 갈 수 없는 곳이다.
    bool floorAt(float x, float y, float& outZ) const;

    // 그 자리에 서 있을 수 있는가. 스폰 자리를 고를 때 쓴다.
    bool blockedAt(float x, float y, const AgentSettings& agent) const;

    // from 에서 to 까지 캡슐 반지름 간격으로 훑는다. 한 틱에 캡슐 지름보다
    // 멀리 움직이면 도착점만 봐서는 얇은 벽을 그냥 지나간다.
    //
    // 높이는 인자로 받지 않는다 — 표본마다 그 자리의 바닥에서 다시 잡는다.
    // 그래서 언덕 위의 벽도 제 높이에서 판정된다.
    bool blockedAlong(float fromX, float fromY, float toX, float toY,
                      const AgentSettings& agent) const;

private:
    // 캡슐 중심이 벽 안에 있는가. 점 하나만 보는 것은 얇은 벽을 놓치므로
    // 밖에서는 blockedAlong 만 쓴다.
    bool blockedByWall(const Vec3& capsuleCenter, const AgentSettings& agent) const;

    std::vector<Obb> walls_;
    Heightmap floor_;
    BoundsSphere bounds_;
};

}  // namespace heaven::instance
