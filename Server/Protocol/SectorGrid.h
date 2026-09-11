#pragma once

// 섹터 격자 계산.
//
// 필드와 인스턴스는 월드 크기와 섹터 크기만 다르고 식은 같다. 상수는 각자
// 헤더에 남기고 (실제로 다른 것이 그것뿐이다) 식은 여기 한 벌만 둔다.
//
// 두 벌로 두면 한쪽만 고쳐진다. clampToWorld 의 epsilon 이 실제로 그렇게
// 갈라졌다 — 3km 월드에서 0.01 은 float 눈금보다 작아 반올림으로 사라진다.

namespace heaven::proto::grid {

// 월드 안으로 민다. 경계 자체는 다음 섹터로 넘어가므로 살짝 안쪽이다.
//
// `value < 0` 이 아니라 `!(value >= 0)` 으로 쓴다. NaN 은 모든 비교가 거짓이라
// 앞의 형태로는 그대로 빠져나가고, sectorIndex 의 float->int 변환이 정의되지
// 않은 값을 내서 섹터 배열을 범위 밖에서 건드리게 된다.
//
// epsilon 은 호출자가 준다. float 의 눈금이 월드 크기에 따라 달라서, 한 값을
// 둘 다에 쓰면 큰 쪽에서 반올림에 먹힌다.
inline constexpr float clampToWorld(float value, float worldSize, float epsilon) {
    if (!(value >= 0.f)) return 0.f;
    if (value >= worldSize) return worldSize - epsilon;
    return value;
}

// 좌표가 [0, worldSize) 로 클램프된 뒤에만 부를 것. 그러지 않으면 결과가
// 섹터 배열 밖을 가리킨다.
inline constexpr int sectorIndex(float x, float y, float sectorSize, int cols) {
    const int col = static_cast<int>(x / sectorSize);
    const int row = static_cast<int>(y / sectorSize);
    return row * cols + col;
}

// 자기 섹터와 8이웃. 월드 밖은 건너뛴다.
template <typename Fn>
inline void forEachNeighborSector(int index, int cols, int rows, Fn&& fn) {
    const int col = index % cols;
    const int row = index / cols;
    for (int dr = -1; dr <= 1; ++dr) {
        const int r = row + dr;
        if (r < 0 || r >= rows) {
            continue;
        }
        for (int dc = -1; dc <= 1; ++dc) {
            const int c = col + dc;
            if (c < 0 || c >= cols) {
                continue;
            }
            fn(r * cols + c);
        }
    }
}

inline constexpr float distanceSquared(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

}  // namespace heaven::proto::grid
