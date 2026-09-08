#pragma once

#include <cstdint>

namespace heaven::fieldshared {

// 클라이언트 포켓몬 DataAsset에서 측정한 달리기 보폭과 맞춘 서버 권위 이동속도.
float pokemonMoveSpeed(std::uint16_t species);

}  // namespace heaven::fieldshared
