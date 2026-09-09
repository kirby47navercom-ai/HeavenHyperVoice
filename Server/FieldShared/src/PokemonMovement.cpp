#include "PokemonMovement.h"

namespace heaven::fieldshared {

float pokemonMoveSpeed(std::uint16_t species) {
    switch (species) {
    case 1:  return 44.09f;   // 귀뚤뚜기
    case 3:  return 98.05f;   // 꼬링크
    case 4:  return 80.08f;   // 꼬부기
    case 5:  return 29.47f;   // 꽁어름
    case 7:  return 25.27f;   // 랄토스
    case 8:  return 60.93f;   // 모부기
    case 9:  return 66.50f;   // 벼리짱
    case 10: return 86.41f;   // 불꽃숭이
    case 12: return 68.42f;   // 이브이
    case 13: return 108.51f;  // 이상해씨
    case 14: return 113.51f;  // 자망칼
    case 15: return 74.17f;   // 터검니
    case 16: return 101.33f;  // 파이리
    case 17: return 45.66f;   // 파치리스
    case 18: return 95.56f;   // 팽도리
    case 20: return 60.90f;   // 피카츄

    // 불꽃/물 계열. 위쪽 값들은 달리기 보폭을 재서 나온 소수인데, 이 묶음은
    // 클라이언트 DataAsset 의 MoveSpeed 에 손으로 정해 둔 값이라 정수다.
    case 27: return 160.0f;   // 개굴반장
    case 28: return 180.0f;   // 갸라도스
    case 29: return 95.0f;    // 누오
    case 30: return 90.0f;    // 블로스터
    case 31: return 160.0f;   // 영치코
    case 32: return 280.0f;   // 윈디
    case 33: return 200.0f;   // 파이어로
    case 34: return 130.0f;   // 폭타
    case 35: return 56.0f;    // 나무지기
    default: return 100.0f;   // 아직 보폭을 확정하지 않은 종의 임시 안전 속도
    }
}

}  // namespace heaven::fieldshared
