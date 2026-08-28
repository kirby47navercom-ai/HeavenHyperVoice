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
    // 서버 내부 번호이자 DB 의 character_pokemon.species_id. 배열 인덱스 + 1 이다.
    std::uint16_t id;

    // 도감번호. 와이어에는 이것만 실어 보낸다.
    //
    // 클라이언트는 이 번호로 종족 에셋을 찾는다. 내부 id 를 보내면 클라이언트
    // 카탈로그 배열 순서가 이 표와 정확히 같아야 하는데, 팀원이 포켓몬을 추가하며
    // 순서가 밀려 이미 저장된 파트너가 다른 종족이 된 적이 있다. 도감번호는
    // 에셋에 이미 박혀 있고(pm0004 = 파이리) 배열 순서와 무관하다.
    std::uint16_t dex;

    std::string_view name;
    std::uint16_t hp;
    std::uint16_t atk;
    std::uint16_t def;
    std::uint16_t spAtk;
    std::uint16_t spDef;
    std::uint16_t speed;
};

// id 는 1 부터 연속이며 아래 배열의 인덱스 + 1 이다.
//
// 순서는 Client/Content/Pokemon/Asset 의 폴더 순서(가나다)와 같게 둔다. 클라가
// 종족 id 로 에셋 폴더를 찾아가므로, 여기 순서를 바꾸면 모델이 통째로 밀린다.
//
// 주의: id 는 DB 의 pokemon.species_id 에 그대로 저장된다. 배열 중간에 끼워
// 넣거나 순서를 바꾸면 이미 저장된 포켓몬이 조용히 다른 종족이 된다.
// 늘릴 때는 반드시 뒤에 붙일 것.
inline constexpr SpeciesBase kSpecies[] = {
    {1,  401, "귀뚤뚜기",   37,  25,  41,  25,  41,  25},
    {2,  487, "기라티나",  150, 100, 120, 100, 120,  90},
    {3,  403, "꼬링크",     45,  65,  34,  40,  34,  45},
    {4,    7, "꼬부기",     44,  48,  65,  50,  64,  43},
    {5,  749, "꽁어름",     70,  80,  70,  80,  70,  70},
    {6,  483, "디아루가",  100, 120, 120, 150, 100,  90},
    {7,  280, "랄토스",     28,  25,  25,  45,  35,  40},
    {8,  387, "모부기",     55,  68,  64,  45,  55,  31},
    {9, 1105, "벼리짱",     70,  80,  70,  80,  70,  70},
    {10,  390, "불꽃숭이",   44,  58,  44,  58,  44,  61},
    {11,  493, "아르세우스",120, 120, 120, 120, 120, 120},
    {12,  133, "이브이",     55,  55,  50,  45,  65,  55},
    {13,    1, "이상해씨",   45,  49,  49,  65,  65,  45},
    {14,  624, "자망칼",     45,  85,  70,  40,  40,  60},
    {15,  610, "터검니",     70,  80,  70,  80,  70,  70},
    {16,    4, "파이리",     39,  52,  43,  60,  50,  65},
    {17,  417, "파치리스",   60,  45,  70,  45,  90,  95},
    {18,  393, "팽도리",     53,  51,  53,  61,  56,  40},
    {19,  484, "펄기아",     90, 120, 100, 150, 120, 100},
    {20,   25, "피카츄",     35,  55,  40,  50,  50,  90},
};

inline constexpr std::size_t kSpeciesCount = sizeof(kSpecies) / sizeof(kSpecies[0]);
inline constexpr std::uint32_t kStarterLevel = 5;

// 개체값은 스탯당 0~31 이고 태어날 때 정해져 바뀌지 않는다.
//
// 노력치 상한(스탯당 252, 합계 510)과 레벨 상한(100)은 여기 두지 않는다.
// 올릴 경로가 아직 없어 강제할 곳도 없었고, 쓰이지 않는 상수는 "검사하고
// 있다" 는 착각만 만든다. 노력치를 올리는 코드가 생길 때 그 옆에 둘 것.
inline constexpr std::uint8_t kMaxIndividualValue = 31;

// 해금 비트맵의 크기. DB 의 character_unlocks.dex_bits BINARY(160) 과 같아야 한다.
// 1280 비트라 현재 최대 도감번호 1105 를 담고도 남는다.
inline constexpr std::size_t kUnlockBitmapBytes = 160;

// 내부 번호를 도감번호로 바꾼다. 모르는 종족이면 0.
// 와이어에는 언제나 도감번호가 나간다 — 내부 번호는 배열 순서라서, 표에 한 줄
// 끼워 넣으면 이미 저장된 종족이 통째로 밀린다.
inline constexpr std::uint16_t dexOf(std::uint16_t id);

// 알 수 없는 종족이면 nullptr. 클라이언트가 보낸 값은 반드시 이걸로 거른다.
inline constexpr const SpeciesBase* findSpecies(std::uint16_t id) {
    if (id == 0 || id > kSpeciesCount) {
        return nullptr;
    }
    // id 가 인덱스 + 1 이라는 전제. 배열을 손댈 때 어긋나면 여기서 걸린다.
    const SpeciesBase& found = kSpecies[id - 1];
    return found.id == id ? &found : nullptr;
}

inline constexpr std::uint16_t dexOf(std::uint16_t id) {
    const SpeciesBase* species = findSpecies(id);
    return species != nullptr ? species->dex : std::uint16_t{0};
}

// 도감번호로 찾는다. 클라이언트가 보내는 것은 이 번호다.
//
// 선형 탐색인 이유는 도감번호가 연속이 아니기 때문이다(꼬부기 7, 피카츄 25,
// 벼리짱 1105). 20 종이라 인덱스를 따로 둘 이유가 없다.
inline constexpr const SpeciesBase* findSpeciesByDex(std::uint16_t dex) {
    if (dex == 0) {
        return nullptr;
    }
    for (const SpeciesBase& species : kSpecies) {
        if (species.dex == dex) {
            return &species;
        }
    }
    return nullptr;
}

namespace detail {

// 도감번호가 겹치면 findSpeciesByDex 가 먼저 나온 것만 돌려주고, 나머지 종족은
// 클라이언트에서 영영 찾을 수 없게 된다. 컴파일 시점에 막는다.
inline constexpr bool dexNumbersAreUnique() {
    for (std::size_t i = 0; i < kSpeciesCount; ++i) {
        if (kSpecies[i].dex == 0) {
            return false;
        }
        for (std::size_t k = i + 1; k < kSpeciesCount; ++k) {
            if (kSpecies[i].dex == kSpecies[k].dex) {
                return false;
            }
        }
    }
    return true;
}
static_assert(dexNumbersAreUnique(), "도감번호가 비었거나 중복이다");

}  // namespace detail

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
//
// 종족값을 여기 박아 둔다. kSpecies 에서 꺼내 쓰면 목록에 종족을 넣고 빼는
// 것만으로 공식과 무관한 검사가 깨진다 (실제로 그렇게 깨진 적이 있다).
// 이건 공식 검증이지 목록 검증이 아니다.
constexpr SpeciesBase kFormulaPikachu{0, 0, "피카츄", 35, 55, 40, 50, 50, 90};
constexpr SpeciesBase kFormulaSnorlax{0, 0, "잠만보", 160, 110, 65, 65, 110, 30};

// 개체값 0 / 노력치 0 은 종족값만 반영된다.
constexpr PokemonStats kPikachuLv5Bare = computeStats(kFormulaPikachu, 5, {}, {});
static_assert(kPikachuLv5Bare.maxHp == 18 && kPikachuLv5Bare.atk == 10 &&
                  kPikachuLv5Bare.def == 9 && kPikachuLv5Bare.speed == 14,
              "레벨 5 최소 개체 계산이 틀렸다");

// 개체값 만렙 + 노력치 252 는 상한 쪽 경계다.
constexpr StatSpread kPerfect{31, 31, 31, 31, 31, 31};
constexpr PokemonStats kPikachuLv50Max =
    computeStats(kFormulaPikachu, 50, kPerfect, {252, 252, 252, 252, 252, 252});
static_assert(kPikachuLv50Max.maxHp == 142 && kPikachuLv50Max.atk == 107 &&
                  kPikachuLv50Max.speed == 142,
              "노력치 반영이 틀렸다 (floor(EV/4) 를 확인할 것)");

// 잠만보는 HP 종족값이 극단적이라 자릿수 실수를 잘 드러낸다.
constexpr PokemonStats kSnorlaxLv100 = computeStats(kFormulaSnorlax, 100, kPerfect, {});
static_assert(kSnorlaxLv100.maxHp == 461 && kSnorlaxLv100.atk == 256,
              "레벨 100 계산이 틀렸다");

static_assert(findSpecies(0) == nullptr, "0 은 유효한 종족이 아니다");
static_assert(findSpecies(kSpeciesCount + 1) == nullptr, "범위 밖 종족은 걸러야 한다");
static_assert(findSpecies(7) != nullptr && findSpecies(7)->id == 7, "id 가 인덱스와 어긋났다");
}  // namespace detail

}  // namespace heaven::proto
