#pragma once

// 필드의 좌표계와 격자.
//
// 단위는 Unreal 과 같은 uu(= 1 cm)다. 클라가 UE5 라 변환을 끼우면 버그만 는다.
// 좌표는 [0, kWorldSize) 이며 음수를 쓰지 않는다 — 섹터 인덱스가 나눗셈 한 번이면 끝난다.
//
// 높이는 아직 없다. 넣을 때 z 를 추가하면 되고, 섹터는 2D 로 남는 편이 낫다
// (수직으로 나눌 만큼 높이가 깊지 않다).

#include <chrono>
#include <cstdint>
#include <limits>

namespace heaven::proto {

inline constexpr float kWorldSize = 51200.f;   // 512 m 정사각형
inline constexpr float kSectorSize = 3200.f;   // 32 m
inline constexpr int kSectorCols = 16;
inline constexpr int kSectorRows = 16;
inline constexpr int kSectorCount = kSectorCols * kSectorRows;

// 시야. 섹터는 후보를 추리는 broad phase 이고, 실제 판정은 이 반경이 한다.
//
// 들어오는 반경과 나가는 반경을 다르게 둔다. 하나면 정확히 그 거리에서
// 서성이는 플레이어가 매 틱 Spawn/Despawn 을 만들어 20Hz 로 깜빡인다.
inline constexpr float kEnterRadius = 2800.f;  // 28 m
inline constexpr float kExitRadius = 3200.f;   // 32 m

// 이동 검증. 클라가 보낸 좌표를 그대로 믿으면 순간이동이 통한다.
// 거절이 아니라 클램프다 — 랙 스파이크로 정상 유저를 튕기지 않는다.
inline constexpr float kMaxSpeed = 600.f;      // 6 m/s, 달리기
inline constexpr float kSpeedSlack = 200.f;    // 지터 여유 2 m

// 이동을 받아들이는 최소 간격. 거리는 클램프해도 **빈도**를 막지 않으면
// 클라 하나가 회선 속도로 Move 를 밀어 넣어 월드 락을 독점할 수 있다.
// 틱이 20Hz 라 그보다 자주 받아봐야 나가는 스냅샷은 늘지 않는다.
inline constexpr std::chrono::milliseconds kMinMoveInterval{10};

inline constexpr float kSpawnX = kWorldSize / 2.f;
inline constexpr float kSpawnY = kWorldSize / 2.f;

// 틱 주기. 이동을 받은 즉시 중계하면 N 명이 서로에게 N × 20 × N 패킷이 된다.
inline constexpr int kTickHz = 20;

// 3×3 후보가 시야를 반드시 덮어야 한다. 이게 깨지면 후보에 없는 엔티티가
// 시야 안에 생겨서 조용히 안 보인다.
static_assert(kEnterRadius <= kSectorSize,
              "시야 반경이 섹터보다 크면 3x3 후보로 부족하다");
static_assert(kEnterRadius < kExitRadius,
              "히스테리시스가 없으면 경계에서 Spawn/Despawn 이 깜빡인다");
static_assert(kSectorCols * kSectorSize == kWorldSize, "격자와 월드 크기가 안 맞는다");

inline constexpr float clampToWorld(float value) {
    // 경계 자체는 다음 섹터로 넘어가므로 아주 살짝 안쪽으로 민다.
    constexpr float kEpsilon = 0.01f;
    // `value < 0` 이 아니라 `!(value >= 0)` 으로 쓴다. NaN 은 모든 비교가 거짓이라
    // 앞의 형태로는 그대로 빠져나가고, sectorIndex 의 float->int 변환이 정의되지
    // 않은 값을 내서 섹터 배열을 범위 밖에서 건드리게 된다.
    if (!(value >= 0.f)) return 0.f;
    if (value >= kWorldSize) return kWorldSize - kEpsilon;
    return value;
}

// 섹터 인덱스가 배열 안에 있으려면 위 클램프가 셋 다 지켜야 한다.
static_assert(clampToWorld(-1.f) == 0.f, "음수는 0 으로");
static_assert(clampToWorld(kWorldSize) < kWorldSize, "상한은 배열 안쪽으로");
static_assert(clampToWorld(std::numeric_limits<float>::quiet_NaN()) == 0.f, "NaN 은 0 으로");

inline constexpr int sectorCol(float x) { return static_cast<int>(x / kSectorSize); }
inline constexpr int sectorRow(float y) { return static_cast<int>(y / kSectorSize); }

inline constexpr int sectorIndex(float x, float y) {
    return sectorRow(y) * kSectorCols + sectorCol(x);
}

// 자기 섹터와 8이웃. 월드 밖은 건너뛴다.
template <typename Fn>
inline void forEachNeighborSector(int index, Fn&& fn) {
    const int col = index % kSectorCols;
    const int row = index / kSectorCols;
    for (int dr = -1; dr <= 1; ++dr) {
        const int r = row + dr;
        if (r < 0 || r >= kSectorRows) {
            continue;
        }
        for (int dc = -1; dc <= 1; ++dc) {
            const int c = col + dc;
            if (c < 0 || c >= kSectorCols) {
                continue;
            }
            fn(r * kSectorCols + c);
        }
    }
}

inline constexpr float distanceSquared(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

}  // namespace heaven::proto
