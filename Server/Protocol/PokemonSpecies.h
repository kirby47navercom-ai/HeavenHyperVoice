#pragma once

// 종족 기본 스탯.
//
// 이건 유저 데이터가 아니라 바뀌지 않는 게임 데이터라 DB 에 두지 않는다.
// 클라이언트도 같은 데이터(모델, 이름)가 필요하므로 어차피 DB 밖에 있어야 한다.
//
// 개체를 만들 때 여기서 실제 스탯을 계산해 character_pokemon 에 **확정 저장**한다.
// 나중에 밸런스를 조정해도 기존 개체가 흔들리지 않고, 개체값을 도입해 같은
// 종족이라도 값이 달라질 때 스키마가 바뀌지 않는다.
//
// DataBase/005_character_pokemon.sql 의 백필이 같은 공식을 쓴다. 한쪽만 고치면
// 마이그레이션으로 만든 개체와 서버가 만든 개체의 스탯이 달라진다.

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

// 스타터 후보. id 는 1 부터 연속이며 아래 배열의 인덱스 + 1 이다.
inline constexpr SpeciesBase kSpecies[] = {
    { 1, "이상해씨",  45, 49, 49, 65, 65, 45},
    { 2, "파이리",    39, 52, 43, 60, 50, 65},
    { 3, "꼬부기",    44, 48, 65, 50, 64, 43},
    { 4, "피카츄",    35, 55, 40, 50, 50, 90},
    { 5, "이브이",    55, 55, 50, 45, 65, 55},
    { 6, "구구",      40, 45, 40, 35, 35, 56},
    { 7, "꼬렛",      30, 56, 35, 25, 35, 72},
    { 8, "모래두지",  50, 75, 85, 20, 30, 40},
    { 9, "나옹",      40, 45, 35, 40, 40, 90},
    {10, "고라파덕",  50, 52, 48, 65, 50, 55},
};

inline constexpr std::size_t kSpeciesCount = sizeof(kSpecies) / sizeof(kSpecies[0]);
inline constexpr std::uint32_t kStarterLevel = 5;

// 알 수 없는 종족이면 nullptr. 클라이언트가 보낸 값은 반드시 이걸로 거른다.
inline constexpr const SpeciesBase* findSpecies(std::uint16_t id) {
    if (id == 0 || id > kSpeciesCount) {
        return nullptr;
    }
    // id 가 인덱스 + 1 이라는 전제. 배열을 손댈 때 어긋나면 여기서 걸린다.
    const SpeciesBase& found = kSpecies[id - 1];
    return found.id == id ? &found : nullptr;
}

// 개체의 실제 스탯. 개체값과 노력치는 아직 없어서 0 으로 둔다.
struct PokemonStats {
    std::uint16_t maxHp = 0;
    std::uint16_t atk = 0;
    std::uint16_t def = 0;
    std::uint16_t spAtk = 0;
    std::uint16_t spDef = 0;
    std::uint16_t speed = 0;
};

// 표준 공식 (개체값/노력치 0):
//   HP    = floor(2 * 종족값 * 레벨 / 100) + 레벨 + 10
//   나머지 = floor(2 * 종족값 * 레벨 / 100) + 5
// 정수 나눗셈이 곧 floor 이므로 그대로 쓴다.
inline constexpr PokemonStats computeStats(const SpeciesBase& base, std::uint32_t level) {
    const auto scale = [level](std::uint16_t value) -> std::uint32_t {
        return 2u * value * level / 100u;
    };
    PokemonStats stats;
    stats.maxHp = static_cast<std::uint16_t>(scale(base.hp) + level + 10u);
    stats.atk = static_cast<std::uint16_t>(scale(base.atk) + 5u);
    stats.def = static_cast<std::uint16_t>(scale(base.def) + 5u);
    stats.spAtk = static_cast<std::uint16_t>(scale(base.spAtk) + 5u);
    stats.spDef = static_cast<std::uint16_t>(scale(base.spDef) + 5u);
    stats.speed = static_cast<std::uint16_t>(scale(base.speed) + 5u);
    return stats;
}

// 005 백필이 SQL 에서 같은 계산을 한다. 두 구현이 어긋나면 마이그레이션으로
// 만들어진 개체와 서버가 만든 개체의 스탯이 달라지는데, 실행해 보기 전에는
// 눈에 띄지 않는다. 컴파일 시점에 잡는다.
namespace detail {
constexpr PokemonStats kBulbasaurLv5 = computeStats(kSpecies[0], kStarterLevel);
static_assert(kBulbasaurLv5.maxHp == 19 && kBulbasaurLv5.atk == 9 &&
                  kBulbasaurLv5.def == 9 && kBulbasaurLv5.spAtk == 11 &&
                  kBulbasaurLv5.spDef == 11 && kBulbasaurLv5.speed == 9,
              "레벨 스탯 공식이 005_character_pokemon.sql 의 백필과 어긋났다");

// 가장 치우친 종족으로 반올림 경계도 같이 확인한다.
constexpr PokemonStats kSandshrewLv5 = computeStats(kSpecies[7], kStarterLevel);
static_assert(kSandshrewLv5.maxHp == 20 && kSandshrewLv5.atk == 12 &&
                  kSandshrewLv5.def == 13 && kSandshrewLv5.spAtk == 7 &&
                  kSandshrewLv5.spDef == 8 && kSandshrewLv5.speed == 9,
              "레벨 스탯 공식이 005_character_pokemon.sql 의 백필과 어긋났다");

static_assert(findSpecies(0) == nullptr, "0 은 유효한 종족이 아니다");
static_assert(findSpecies(kSpeciesCount + 1) == nullptr, "범위 밖 종족은 걸러야 한다");
static_assert(findSpecies(4) != nullptr && findSpecies(4)->id == 4, "id 가 인덱스와 어긋났다");
}  // namespace detail

}  // namespace heaven::proto
