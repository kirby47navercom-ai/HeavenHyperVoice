#include "GachaPool.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "PokemonSpecies.h"

namespace heaven::proto {

namespace {

GachaType typeFromName(const std::string& name) {
    for (std::uint8_t value = 1; value <= kGachaTypeCount; ++value) {
        const GachaType type = static_cast<GachaType>(value);
        if (name == gachaTypeName(type)) {
            return type;
        }
    }
    return GachaType::None;
}

bool rarityFromName(const std::string& name, GachaRarity& out) {
    if (name == "normal") {
        out = GachaRarity::Normal;
        return true;
    }
    if (name == "rare") {
        out = GachaRarity::Rare;
        return true;
    }
    if (name == "super") {
        out = GachaRarity::SuperRare;
        return true;
    }
    return false;
}

std::size_t indexOf(GachaType type) {
    return static_cast<std::size_t>(static_cast<std::uint8_t>(type) - 1);
}

}  // namespace

bool GachaTable::loadFromFile(const std::string& path, std::string& error) {
    pools_ = {};
    sums_ = {};
    loaded_ = false;

    std::ifstream in(path);
    if (!in) {
        error = "cannot open " + path;
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (!parseLine(line, lineNumber, error)) {
            pools_ = {};
            sums_ = {};
            return false;
        }
    }

    // 비어 있는 타입을 그냥 두면 그 버튼만 조용히 안 된다. 기동할 때 막는다.
    for (std::uint8_t value = 1; value <= kGachaTypeCount; ++value) {
        const GachaType type = static_cast<GachaType>(value);
        if (sums_[indexOf(type)] == 0) {
            error = std::string("gacha type '") + gachaTypeName(type) +
                    "' has no candidate with a positive weight";
            pools_ = {};
            sums_ = {};
            return false;
        }
    }

    loaded_ = true;
    return true;
}

bool GachaTable::parseLine(const std::string& line, int lineNumber, std::string& error) {
    std::istringstream stream(line);
    std::string directive;
    stream >> directive;
    if (directive.empty() || directive[0] == '#') {
        return true;
    }

    const std::string at = " at line " + std::to_string(lineNumber);

    if (directive != "entry") {
        error = "unknown gacha directive '" + directive + "'" + at;
        return false;
    }

    std::string typeName;
    std::string rarityName;
    unsigned dex = 0;
    unsigned weight = 0;
    if (!(stream >> typeName >> dex >> rarityName >> weight)) {
        error = "malformed entry" + at;
        return false;
    }

    const GachaType type = typeFromName(typeName);
    if (type == GachaType::None) {
        error = "unknown gacha type '" + typeName + "'" + at;
        return false;
    }

    GachaRarity rarity = GachaRarity::Normal;
    if (!rarityFromName(rarityName, rarity)) {
        error = "unknown rarity '" + rarityName + "' (normal|rare|super)" + at;
        return false;
    }

    // 지급할 수 없는 후보를 굴리면 꽝도 아닌 조용한 실패가 된다. 여기서 막는다.
    if (dex == 0 || dex > 0xFFFFu || findSpeciesByDex(static_cast<std::uint16_t>(dex)) == nullptr) {
        error = "dex " + std::to_string(dex) + " is not in the species table" + at;
        return false;
    }
    // 레이드에서만 만나는 것이 그 종족의 값어치다. 뽑기로 새게 하지 않는다.
    if (isBossDex(static_cast<std::uint16_t>(dex))) {
        error = "dex " + std::to_string(dex) + " is a raid boss and cannot be drawn" + at;
        return false;
    }

    std::vector<GachaEntry>& pool = pools_[indexOf(type)];
    const auto duplicate = std::find_if(pool.begin(), pool.end(), [dex](const GachaEntry& entry) {
        return entry.dex == dex;
    });
    if (duplicate != pool.end()) {
        error = "dex " + std::to_string(dex) + " appears twice in '" + typeName + "'" + at;
        return false;
    }

    // weight 0 은 "후보에서 뺀다" 다. 담아 두면 합을 0 으로 나누는 일만 생긴다.
    if (weight == 0) {
        return true;
    }

    pool.push_back({static_cast<std::uint16_t>(dex), rarity, weight});
    sums_[indexOf(type)] += weight;
    return true;
}

GachaDraw GachaTable::draw(GachaType type, std::mt19937& rng) const {
    if (!loaded_ || !isGachaType(static_cast<std::uint8_t>(type))) {
        return {};
    }

    const std::size_t index = indexOf(type);
    const std::uint32_t sum = sums_[index];
    if (sum == 0) {
        return {};
    }

    // [1, sum] 에서 뽑아 누적 가중치를 넘어서는 첫 후보를 고른다.
    std::uniform_int_distribution<std::uint32_t> pick(1, sum);
    std::uint32_t roll = pick(rng);
    for (const GachaEntry& entry : pools_[index]) {
        if (roll <= entry.weight) {
            return {entry.dex, entry.rarity};
        }
        roll -= entry.weight;
    }

    // 합이 실제 가중치보다 크지 않으면 여기 오지 않는다. 와도 마지막을 준다.
    const GachaEntry& last = pools_[index].back();
    return {last.dex, last.rarity};
}

std::string GachaTable::describe() const {
    if (!loaded_) {
        return "not loaded";
    }

    std::string out;
    for (std::uint8_t value = 1; value <= kGachaTypeCount; ++value) {
        const GachaType type = static_cast<GachaType>(value);
        const std::size_t index = indexOf(type);
        if (!out.empty()) {
            out += ", ";
        }
        out += gachaTypeName(type);
        out += " " + std::to_string(pools_[index].size());
        out += "/" + std::to_string(sums_[index]);
    }
    return out;
}

}  // namespace heaven::proto
