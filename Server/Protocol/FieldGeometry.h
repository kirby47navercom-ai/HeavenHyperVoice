#pragma once

// 필드의 좌표계와 격자.
//
// 단위는 Unreal 과 같은 uu(= 1 cm)다. 클라가 UE5 라 변환을 끼우면 버그만 는다.
// 좌표는 [0, kWorldSize) 이며 음수를 쓰지 않는다 — 섹터 인덱스가 나눗셈 한 번이면 끝난다.
//
// 높이(z)는 위치에 보관하고 와이어에도 싣지만, 섹터는 2D 로 남긴다.
// 수직으로 나눌 만큼 높이가 깊지 않고 broad phase 는 수평 거리만 추린다.

#include <chrono>
#include <cstdint>
#include <limits>

#include "SectorGrid.h"

namespace heaven::proto {

inline constexpr float kWorldSize = 51200.f; // 512 m 정사각형
inline constexpr float kSectorSize = 3200.f; // 32 m
inline constexpr int kSectorCols = 16;
inline constexpr int kSectorRows = 16;
inline constexpr int kSectorCount = kSectorCols * kSectorRows;

// 시야. 섹터는 후보를 추리는 broad phase 이고, 실제 판정은 이 반경이 한다.
//
// 들어오는 반경과 나가는 반경을 다르게 둔다. 하나면 정확히 그 거리에서
// 서성이는 플레이어가 매 틱 Spawn/Despawn 을 만들어 20Hz 로 깜빡인다.
inline constexpr float kEnterRadius = 3000.f; // 30 m
inline constexpr float kExitRadius = 5000.f;  // 50 m — 넉넉한 히스테리시스로
                                              // 경계에서 서성이는 것이 매 틱
                                              // 나타났다 사라지지 않게 한다.

inline constexpr float kSpawnX = kWorldSize / 2.f;
inline constexpr float kSpawnY = kWorldSize / 2.f;

// 틱 주기. 이동을 받은 즉시 중계하면 N 명이 서로에게 N × 20 × N 패킷이 된다.
inline constexpr int kTickHz = 20;

// 3×3 후보가 시야를 반드시 덮어야 한다. 이게 깨지면 후보에 없는 엔티티가
// 시야 안에 생겨서 조용히 안 보인다.
static_assert(kEnterRadius <= kSectorSize, "시야 반경이 섹터보다 크면 3x3 후보로 부족하다");
static_assert(kEnterRadius < kExitRadius, "히스테리시스가 없으면 경계에서 Spawn/Despawn 이 깜빡인다");
static_assert(kSectorCols * kSectorSize == kWorldSize, "격자와 월드 크기가 안 맞는다");

// 경계 자체는 다음 섹터로 넘어가므로 아주 살짝 안쪽으로 민다.
// 512m 월드에서 float 눈금은 0.004 라 0.01 이면 충분하다.
inline constexpr float clampToWorld(float value) {
    return grid::clampToWorld(value, kWorldSize, 0.01f);
}

// 섹터 인덱스가 배열 안에 있으려면 위 클램프가 셋 다 지켜야 한다.
static_assert(clampToWorld(-1.f) == 0.f, "음수는 0 으로");
static_assert(clampToWorld(kWorldSize) < kWorldSize, "상한은 배열 안쪽으로");
static_assert(clampToWorld(std::numeric_limits<float>::quiet_NaN()) == 0.f, "NaN 은 0 으로");

inline constexpr int sectorIndex(float x, float y) {
    return grid::sectorIndex(clampToWorld(x), clampToWorld(y), kSectorSize, kSectorCols);
}

template <typename Fn> inline void forEachNeighborSector(int index, Fn &&fn) {
    grid::forEachNeighborSector(index, kSectorCols, kSectorRows, fn);
}

using grid::distanceSquared;

} // namespace heaven::proto
