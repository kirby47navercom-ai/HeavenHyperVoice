#pragma once

// 타입별 뽑기 후보와 가중치.
//
// 확률 판정은 서버가 한다. 클라이언트는 타입만 보낸다 — 후보 목록은 클라도
// 알고 있으므로(확률 표시에 쓴다) "후보에 있는가" 만 검증하면 후보 안에서
// 슈퍼레어를 100% 지목하는 치팅을 막을 수 없다.
//
// 표를 코드에 박지 않고 데이터 파일에서 읽는 이유는 확률 조정이 서버 재빌드가
// 되면 아무도 안 만지기 때문이다. 형식은 maps/*.hhvmap 과 같은 줄 단위
// 텍스트다 (Nav/src/Map.cpp 참고).
//
// 후보의 값어치는 여기서 지키지 않는다. 도감번호가 종족 표에 있는지와 보스
// 종족이 아닌지를 **읽을 때** 검증하고, 어긋나면 서버가 뜨지 않는다. 굴려 놓고
// 지급할 수 없는 후보는 꽝도 아닌 조용한 실패가 된다.

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace heaven::proto {

enum class GachaRarity : std::uint8_t { Normal = 0, Rare = 1, SuperRare = 2 };

// field.fbs 의 GachaType 과 같은 값이어야 한다. 0 은 "안 정했다" 이고 거절된다 —
// 기본값이 불꽃이 되면 타입을 빠뜨린 클라가 조용히 불꽃을 뽑는다.
enum class GachaType : std::uint8_t {
    None = 0,
    Fire = 1,
    Water = 2,
    Grass = 3,
    Normal = 4,
    Electric = 5,
};

inline constexpr std::size_t kGachaTypeCount = 5;

inline constexpr bool isGachaType(std::uint8_t value) {
    return value >= 1 && value <= kGachaTypeCount;
}

// 데이터 파일에 적는 이름. 로그와 오류 메시지에도 쓴다.
inline constexpr const char* gachaTypeName(GachaType type) {
    switch (type) {
        case GachaType::Fire:     return "fire";
        case GachaType::Water:    return "water";
        case GachaType::Grass:    return "grass";
        case GachaType::Normal:   return "normal";
        case GachaType::Electric: return "electric";
        case GachaType::None:     break;
    }
    return "none";
}

struct GachaEntry {
    std::uint16_t dex = 0;
    GachaRarity rarity = GachaRarity::Normal;

    // 그 타입 안에서의 상대 가중치. 합을 1000 으로 맞춰 적으면 천분율이 된다.
    std::uint32_t weight = 0;
};

struct GachaDraw {
    std::uint16_t dex = 0;  // 0 이면 뽑을 것이 없었다
    GachaRarity rarity = GachaRarity::Normal;
};

class GachaTable {
public:
    // 다섯 타입이 모두 후보를 하나 이상 가져야 성공한다. 실패하면 error 에 사유.
    bool loadFromFile(const std::string& path, std::string& error);

    bool loaded() const { return loaded_; }

    // 한 번 추첨한다. rng 는 호출자가 소유한다 — mt19937 은 스레드 안전하지 않고
    // 이 표는 여러 DB 스레드가 함께 읽으므로, 표에 난수원을 두면 안 된다.
    GachaDraw draw(GachaType type, std::mt19937& rng) const;

    // 기동 로그용. "fire 5/1000, water 5/1000, ..."
    std::string describe() const;

private:
    bool parseLine(const std::string& line, int lineNumber, std::string& error);

    // 인덱스는 GachaType 값 - 1 이다.
    std::array<std::vector<GachaEntry>, kGachaTypeCount> pools_;
    std::array<std::uint32_t, kGachaTypeCount> sums_{};
    bool loaded_ = false;
};

}  // namespace heaven::proto
