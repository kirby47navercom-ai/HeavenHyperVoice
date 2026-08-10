#pragma once

// 종족 기본 스탯과 실 수치 계산.
//
// 종족값은 유저 데이터가 아니라 바뀌지 않는 게임 데이터라 DB 에 두지 않는다.
// 클라이언트도 같은 데이터(모델, 이름)가 필요하므로 어차피 DB 밖에 있어야 한다.
//
// 실 수치는 **저장하지 않는다**. 레벨업과 노력치로 계속 바뀌기 때문에, 저장해
// 두면 갱신을 한 군데라도 빠뜨리는 순간 조용히 어긋난다. DB 에는 입력만 두고
// (종족, 레벨, 개체값, 노력치) 읽을 때 여기서 계산한다.

#include <cstdint>
#include <string_view>

namespace heaven::proto {

struct SpeciesBase {
    std::uint16_t id;
    std::string_view name;
    std::uint16_t hp;
    std::uint16_t atk;
    std::uint16_t def;
    std::uint16_t spAtk;
    std::uint16_t spDef;
    std::uint16_t speed;
};

// id 는 1 부터 연속이며 아래 배열의 인덱스 + 1 이다.
inline constexpr SpeciesBase kSpecies[] = {
    { 1, "피카츄",     35,  55,  40,  50,  50,  90},
    { 2, "이상해꽃",   80,  82,  83, 100, 100,  80},
    { 3, "리자몽",     78,  84,  78, 109,  85, 100},
    { 4, "거북왕",     79,  83, 100,  85, 105,  78},
    { 5, "딱구리",     70, 110, 180,  60,  60,  50},
    { 6, "후딘",       55,  50,  45, 135,  95, 120},
    { 7, "잠만보",    160, 110,  65,  65, 110,  30},
    { 8, "포푸니라",   70, 120,  65,  45,  85, 125},
    { 9, "루카리오",   70, 110,  70, 115,  70,  90},
    {10, "귀뚤톡크",   77,  85,  51,  55,  51,  65},
};

inline constexpr std::size_t kSpeciesCount = sizeof(kSpecies) / sizeof(kSpecies[0]);
inline constexpr std::uint32_t kStarterLevel = 5;
inline constexpr std::uint32_t kMaxLevel = 100;

// 개체값은 스탯당 0~31 이고 태어날 때 정해져 바뀌지 않는다.
// 노력치는 스탯당 0~252, 여섯 개 합계 510 이 상한이다.
inline constexpr std::uint8_t kMaxIndividualValue = 31;
inline constexpr std::uint16_t kMaxEffortValue = 252;
inline constexpr std::uint16_t kMaxEffortTotal = 510;

// 알 수 없는 종족이면 nullptr. 클라이언트가 보낸 값은 반드시 이걸로 거른다.
inline constexpr const SpeciesBase* findSpecies(std::uint16_t id) {
    if (id == 0 || id > kSpeciesCount) {
        return nullptr;
    }
    // id 가 인덱스 + 1 이라는 전제. 배열을 손댈 때 어긋나면 여기서 걸린다.
    const SpeciesBase& found = kSpecies[id - 1];
    return found.id == id ? &found : nullptr;
}

// 개체값과 노력치는 같은 모양이라 타입을 공유한다. 범위만 다르다.
struct StatSpread {
    std::uint16_t hp = 0;
    std::uint16_t atk = 0;
    std::uint16_t def = 0;
    std::uint16_t spAtk = 0;
    std::uint16_t spDef = 0;
    std::uint16_t speed = 0;
};

struct PokemonStats {
    std::uint16_t maxHp = 0;
    std::uint16_t atk = 0;
    std::uint16_t def = 0;
    std::uint16_t spAtk = 0;
    std::uint16_t spDef = 0;
    std::uint16_t speed = 0;
};

// 3세대 이후 표준 공식.
//
//   HP     = floor((2 * 종족값 + 개체값 + floor(노력치 / 4)) * 레벨 / 100) + 레벨 + 10
//   그 외   = floor((2 * 종족값 + 개체값 + floor(노력치 / 4)) * 레벨 / 100) + 5
//
// 성격 보정은 아직 없다. 넣을 때는 HP 를 제외한 다섯에 0.9 / 1.0 / 1.1 을 곱하고
// 내림하면 되며, 성격 id 컬럼 하나가 는다.
inline constexpr PokemonStats computeStats(const SpeciesBase& base, std::uint32_t level,
                                           const StatSpread& ivs, const StatSpread& evs) {
    const auto common = [level](std::uint16_t baseValue, std::uint16_t iv,
                                std::uint16_t ev) -> std::uint32_t {
        // 정수 나눗셈이 곧 floor 다.
        return (2u * baseValue + iv + ev / 4u) * level / 100u;
    };

    PokemonStats stats;
    stats.maxHp = static_cast<std::uint16_t>(common(base.hp, ivs.hp, evs.hp) + level + 10u);
    stats.atk = static_cast<std::uint16_t>(common(base.atk, ivs.atk, evs.atk) + 5u);
    stats.def = static_cast<std::uint16_t>(common(base.def, ivs.def, evs.def) + 5u);
    stats.spAtk = static_cast<std::uint16_t>(common(base.spAtk, ivs.spAtk, evs.spAtk) + 5u);
    stats.spDef = static_cast<std::uint16_t>(common(base.spDef, ivs.spDef, evs.spDef) + 5u);
    stats.speed = static_cast<std::uint16_t>(common(base.speed, ivs.speed, evs.speed) + 5u);
    return stats;
}

namespace detail {

// 공식이 흔들리면 개체 스탯이 조용히 달라진다. 컴파일 시점에 잡는다.
// 값은 3세대 이후 계산기와 대조한 것이다.

// 개체값 0 / 노력치 0 은 종족값만 반영된다.
constexpr PokemonStats kPikachuLv5Bare = computeStats(kSpecies[0], 5, {}, {});
static_assert(kPikachuLv5Bare.maxHp == 18 && kPikachuLv5Bare.atk == 10 &&
                  kPikachuLv5Bare.def == 9 && kPikachuLv5Bare.speed == 14,
              "레벨 5 최소 개체 계산이 틀렸다");

// 개체값 만렙 + 노력치 252 는 상한 쪽 경계다.
constexpr StatSpread kPerfect{31, 31, 31, 31, 31, 31};
constexpr PokemonStats kPikachuLv50Max =
    computeStats(kSpecies[0], 50, kPerfect, {252, 252, 252, 252, 252, 252});
static_assert(kPikachuLv50Max.maxHp == 142 && kPikachuLv50Max.atk == 107 &&
                  kPikachuLv50Max.speed == 142,
              "노력치 반영이 틀렸다 (floor(EV/4) 를 확인할 것)");

// 잠만보는 HP 종족값이 극단적이라 자릿수 실수를 잘 드러낸다.
constexpr PokemonStats kSnorlaxLv100 = computeStats(kSpecies[6], 100, kPerfect, {});
static_assert(kSnorlaxLv100.maxHp == 461 && kSnorlaxLv100.atk == 256,
              "레벨 100 계산이 틀렸다");

static_assert(findSpecies(0) == nullptr, "0 은 유효한 종족이 아니다");
static_assert(findSpecies(kSpeciesCount + 1) == nullptr, "범위 밖 종족은 걸러야 한다");
static_assert(findSpecies(7) != nullptr && findSpecies(7)->id == 7, "id 가 인덱스와 어긋났다");
}  // namespace detail

}  // namespace heaven::proto
