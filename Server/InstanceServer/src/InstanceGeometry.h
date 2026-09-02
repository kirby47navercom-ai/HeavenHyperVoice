#pragma once

// 인스턴스의 좌표계와 격자.
//
// Protocol/FieldGeometry.h 에서 갈라져 나왔다. 필드는 512m 정사각형이지만
// 인스턴스 맵은 놀 수 있는 범위가 2km 라 같은 상수를 쓸 수 없다. 와이어 형식은
// 그대로 공유하고(좌표는 그냥 float 다) 여기서 다른 것은 크기와 시야뿐이다.
//
// 단위는 Unreal 과 같은 uu(= 1 cm)다. 좌표는 [0, kWorldSize) 이며 음수를 쓰지
// 않는다 — 섹터 인덱스가 나눗셈 한 번이면 끝난다.

#include <chrono>
#include <cstdint>
#include <limits>

namespace heaven::instance {

// 놀 수 있는 범위는 지름 2km 의 구다 (맵의 bounds_sphere). 월드는 그 구가
// 어디에 놓이든 담기게 여유를 두고 3km 로 잡는다 — 좌표가 음수가 되면
// 섹터 인덱스가 깨지므로 경계에 딱 맞추지 않는다.
inline constexpr float kWorldSize = 307200.f;   // 3.072 km
inline constexpr float kSectorSize = 12800.f;   // 128 m
inline constexpr int kSectorCols = 24;
inline constexpr int kSectorRows = 24;
inline constexpr int kSectorCount = kSectorCols * kSectorRows;

// 언리얼 원점이 월드 한가운데에 오게 하는 값.
//
// 클라이언트는 이 값을 설정 파일이 아니라 EnterAck 으로 받는다. 필드(25600)와
// 인스턴스(153600)가 다른데 ini 두 곳을 손으로 맞추면 언젠가 어긋나고, 어긋나면
// 전원이 엉뚱한 자리에 서는 것으로만 드러난다.
inline constexpr float kWorldOriginOffset = kWorldSize / 2.f;

// 시야. 섹터는 후보를 추리는 broad phase 이고, 실제 판정은 이 반경이 한다.
//
// 필드(30m)보다 넓게 잡는다. 놀 수 있는 범위가 2km 라 30m 로는 같은 방 사람이
// 있어도 서로 못 본다. 들어오는 반경과 나가는 반경을 다르게 두는 이유는 필드와
// 같다 — 하나면 그 거리에서 서성이는 것만으로 매 틱 Spawn/Despawn 이 깜빡인다.
inline constexpr float kEnterRadius = 10000.f;  // 100 m
inline constexpr float kExitRadius = 15000.f;   // 150 m

// 이동 검증. 필드와 같은 값이다 — 캐릭터 이동 속도는 맵 크기와 무관하다.
inline constexpr float kMaxSpeed = 600.f;       // 6 m/s, 달리기
inline constexpr float kSpeedSlack = 200.f;     // 지터 예산 상한
inline constexpr float kSlackRefill = 200.f;    // 초당 회복량
inline constexpr float kMaxMoveElapsed = 1.f;   // 허용 거리를 낼 때 인정하는 경과 시간 상한
inline constexpr std::chrono::milliseconds kMinMoveInterval{10};

// 스폰. 월드 한가운데이자 언리얼 원점이다.
// ponytail: 전원이 같은 점에 뜬다. 맵에 spawn 줄이 생기면 거기서 골라 쓸 것.
inline constexpr float kSpawnX = kWorldSize / 2.f;
inline constexpr float kSpawnY = kWorldSize / 2.f;

inline constexpr int kTickHz = 20;

// 3×3 후보가 시야를 반드시 덮어야 한다. 이게 깨지면 후보에 없는 엔티티가
// 시야 안에 생겨서 조용히 안 보인다.
static_assert(kEnterRadius <= kSectorSize,
              "시야 반경이 섹터보다 크면 3x3 후보로 부족하다");
static_assert(kEnterRadius < kExitRadius,
              "히스테리시스가 없으면 경계에서 Spawn/Despawn 이 깜빡인다");
static_assert(kSectorCols * kSectorSize == kWorldSize, "격자와 월드 크기가 안 맞는다");

inline constexpr float clampToWorld(float value) {
    // 경계 자체는 다음 섹터로 넘어가므로 살짝 안쪽으로 민다.
    //
    // 필드는 0.01 을 쓰지만 여기서는 안 된다. float 는 307200 부근에서 눈금이
    // 0.031 이라 0.01 을 빼면 반올림으로 도로 307200 이 된다 (아래 static_assert
    // 가 이걸 잡았다). 1uu = 1cm 는 3km 월드에서 무시할 만하고 눈금보다 넉넉히 크다.
    constexpr float kEpsilon = 1.f;
    // `value < 0` 이 아니라 `!(value >= 0)` 으로 쓴다. NaN 은 모든 비교가 거짓이라
    // 앞의 형태로는 그대로 빠져나가고, sectorIndex 의 float->int 변환이 정의되지
    // 않은 값을 내서 섹터 배열을 범위 밖에서 건드리게 된다.
    if (!(value >= 0.f)) return 0.f;
    if (value >= kWorldSize) return kWorldSize - kEpsilon;
    return value;
}

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

}  // namespace heaven::instance
