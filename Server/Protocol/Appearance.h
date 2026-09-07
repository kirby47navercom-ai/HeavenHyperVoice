#pragma once

// 캐릭터 외형 하나. 로그인·필드·인스턴스가 모두 이걸 주고받는다.
//
// LoginCodec.h 에 있던 것을 뺐다. 필드 스냅샷에도 외형이 실리게 되면서
// FieldCodec.h 가 같은 타입을 필요로 하는데, 그것 하나 때문에 login_generated.h
// 를 필드 서버로 끌고 들어올 이유가 없다.

#include <cstdint>

namespace heaven::proto {

// 캐릭터 외형. 클라이언트의 FUEHHVAppearance 와 1:1 이며, 기본값도 같게 둔다.
// 서버는 내용을 해석하지 않고 범위만 본다 — 어떤 인덱스가 어떤 머리인지는
// 클라이언트의 카탈로그가 안다.
struct AppearanceInfo {
    std::uint8_t gender = 0;  // 0 = TypeA, 1 = TypeB

    std::int32_t body = 1;
    std::int32_t head = 0;
    std::int32_t hair = 0;
    std::int32_t eye = 0;
    std::int32_t equipment = 1;

    float skinR = 1.0f,      skinG = 0.712f,    skinB = 0.6458f;
    float hairR = 0.1719f,   hairG = 0.1111f,   hairB = 0.0850f;
    float eyeR  = 0.070638f, eyeG  = 0.484375f, eyeB  = 0.243701f;

    float armVolume = 0.f;
    float torsoVolume = 0.f;
    float legVolume = 0.f;
};


// 클라이언트가 보낸 값을 그대로 저장하지 않는다. 인덱스가 음수면 배열 접근이
// 깨지고, 부피가 범위를 벗어나면 모델이 뒤틀린다. 색은 [0,1] 밖이면 발광한다.
//
// 인덱스 상한은 서버가 모른다 — 카탈로그는 클라이언트 에셋이다. 음수만 막고
// 상한은 클라이언트가 표시할 때 클램프한다.
inline void sanitizeAppearance(AppearanceInfo& appearance) {
    const auto clampIndex = [](std::int32_t v) { return v < 0 ? 0 : v; };
    appearance.gender = appearance.gender != 0 ? 1 : 0;
    appearance.body = clampIndex(appearance.body);
    appearance.head = clampIndex(appearance.head);
    appearance.hair = clampIndex(appearance.hair);
    appearance.eye = clampIndex(appearance.eye);
    appearance.equipment = clampIndex(appearance.equipment);

    const auto clampUnit = [](float v) {
        if (!(v >= 0.f)) return 0.f;  // NaN 도 여기서 걸린다
        return v > 1.f ? 1.f : v;
    };
    for (float* channel : {&appearance.skinR, &appearance.skinG, &appearance.skinB,
                           &appearance.hairR, &appearance.hairG, &appearance.hairB,
                           &appearance.eyeR, &appearance.eyeG, &appearance.eyeB}) {
        *channel = clampUnit(*channel);
    }

    const auto clampVolume = [](float v) {
        if (!(v >= -1.f)) return 0.f;  // NaN 은 중립으로
        return v > 1.f ? 1.f : v;
    };
    appearance.armVolume = clampVolume(appearance.armVolume);
    appearance.torsoVolume = clampVolume(appearance.torsoVolume);
    appearance.legVolume = clampVolume(appearance.legVolume);
}

}  // namespace heaven::proto
