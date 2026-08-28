#include "DevStore.h"

#include <algorithm>

namespace heaven::data {

CreateAccountResult DevStore::createAccount(std::string_view, const std::string&) {
    return CreateAccountResult::NotSupported;
}

// 비밀번호는 받지도 않는다. 아이디 형식 검증은 LoginHandler 가 이미 했다.
std::optional<Account> DevStore::authenticate(std::string_view username, std::string_view) {
    if (username.empty()) {
        return std::nullopt;
    }

    // 아이디에서 결정적으로 계정 ID 를 만든다 (FNV-1a).
    // 같은 아이디는 항상 같은 ID 를 받는다.
    std::uint64_t hash = 1469598103934665603ull;
    for (const char c : username) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }

    Account account;
    account.id = hash;
    return account;
}

std::vector<Character> DevStore::listByAccount(std::uint64_t accountId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = characters_.find(accountId);
    return it == characters_.end() ? std::vector<Character>{} : it->second;
}

std::optional<Character> DevStore::find(std::uint64_t accountId, std::uint64_t characterId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = characters_.find(accountId);
    if (it == characters_.end()) {
        return std::nullopt;
    }
    const auto found = std::find_if(it->second.begin(), it->second.end(),
                                    [characterId](const Character& c) {
                                        return c.id == characterId;
                                    });
    return found == it->second.end() ? std::nullopt : std::optional<Character>(*found);
}

CreateCharacterResult DevStore::create(std::uint64_t accountId, std::string_view nickname,
                                       std::uint16_t speciesId,
                                       const Appearance& appearance) {
    // 0 은 "파트너 없이 시작" 이다.
    const proto::SpeciesBase* species =
        speciesId == 0 ? nullptr : proto::findSpecies(speciesId);
    if (speciesId != 0 && species == nullptr) {
        return CreateCharacterResult::UnknownSpecies;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Character>& owned = characters_[accountId];
    if (owned.size() >= kMaxCharactersPerAccount) {
        return CreateCharacterResult::SlotsFull;
    }

    // 실제 DB 는 전역 유일 제약이지만 여기서는 이 계정 안에서만 본다.
    // 개발용이라 그 정도로 충분하다.
    const bool taken = std::any_of(owned.begin(), owned.end(), [&](const Character& c) {
        return c.nickname == nickname;
    });
    if (taken) {
        return CreateCharacterResult::NicknameTaken;
    }

    Character character;
    character.id = nextCharacterId_++;
    character.nickname = std::string(nickname);
    character.level = proto::kStarterLevel;
    character.appearance = appearance;
    if (species != nullptr) {
        unlocks_[character.id].insert(speciesId);
        character.unlocked.push_back(species->dex);
        character.party.push_back(species->dex);
        character.hasPartner = true;
        character.partner.speciesId = speciesId;
        character.partner.name = std::string(species->name);
        character.partner.stats = proto::computeStats(*species, character.level, {}, {});
    }
    owned.push_back(std::move(character));

    return CreateCharacterResult::Created;
}

Character* DevStore::findLocked(std::uint64_t accountId, std::uint64_t characterId) {
    const auto owned = characters_.find(accountId);
    if (owned == characters_.end()) {
        return nullptr;
    }
    for (Character& character : owned->second) {
        if (character.id == characterId) {
            return &character;
        }
    }
    return nullptr;
}

bool DevStore::unlockSpecies(std::uint64_t accountId, std::uint64_t characterId,
                             std::uint16_t speciesId) {
    if (proto::findSpecies(speciesId) == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (findLocked(accountId, characterId) == nullptr) {
        return false;
    }
    unlocks_[characterId].insert(speciesId);
    return true;
}

DeleteResult DevStore::setActivePartner(std::uint64_t accountId, std::uint64_t characterId,
                                        std::uint16_t speciesId) {
    const proto::SpeciesBase* species = proto::findSpecies(speciesId);
    if (species == nullptr) {
        return DeleteResult::NotFound;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    Character* character = findLocked(accountId, characterId);
    if (character == nullptr) {
        return DeleteResult::NotFound;
    }

    // 해금하지 않은 종족은 거절한다. 개발용이라도 여기를 열어 두면 클라이언트
    // 쪽 버그가 dev 에서만 조용히 지나간다.
    const auto unlocked = unlocks_.find(characterId);
    if (unlocked == unlocks_.end() || unlocked->second.count(speciesId) == 0) {
        return DeleteResult::NotUnlocked;
    }

    // 꺼낼 수 있는 것은 파티 안에 있는 것뿐이다.
    if (std::find(character->party.begin(), character->party.end(), species->dex)
        == character->party.end()) {
        return DeleteResult::NotUnlocked;
    }

    character->hasPartner = true;
    character->partner.speciesId = speciesId;
    character->partner.name = std::string(species->name);
    character->partner.stats = proto::computeStats(*species, character->level, {}, {});
    return DeleteResult::Deleted;
}

PartyResult DevStore::setParty(std::uint64_t accountId, std::uint64_t characterId,
                              const std::vector<std::uint16_t>& dexNumbers,
                              std::uint16_t activeDex) {
    if (dexNumbers.size() > kMaxPartySize) {
        return PartyResult::TooMany;
    }
    for (std::size_t i = 0; i < dexNumbers.size(); ++i) {
        if (proto::findSpeciesByDex(dexNumbers[i]) == nullptr) {
            return PartyResult::NotUnlocked;
        }
        for (std::size_t j = i + 1; j < dexNumbers.size(); ++j) {
            if (dexNumbers[i] == dexNumbers[j]) {
                return PartyResult::Duplicate;
            }
        }
    }
    if (activeDex != 0 &&
        std::find(dexNumbers.begin(), dexNumbers.end(), activeDex) == dexNumbers.end()) {
        return PartyResult::NotInParty;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    Character* character = findLocked(accountId, characterId);
    if (character == nullptr) {
        return PartyResult::NotFound;
    }

    // 해금 목록은 내부 번호로 들고 있고 요청은 도감번호다. dev 라도 이 확인을
    // 빼면 클라이언트 버그가 dev 에서만 조용히 지나간다.
    const auto unlocked = unlocks_.find(characterId);
    for (const std::uint16_t dex : dexNumbers) {
        const proto::SpeciesBase* species = proto::findSpeciesByDex(dex);
        if (unlocked == unlocks_.end() || unlocked->second.count(species->id) == 0) {
            return PartyResult::NotUnlocked;
        }
    }

    character->party.assign(dexNumbers.begin(), dexNumbers.end());
    character->hasPartner = false;
    character->partner = {};
    if (activeDex != 0) {
        const proto::SpeciesBase* species = proto::findSpeciesByDex(activeDex);
        character->hasPartner = true;
        character->partner.speciesId = species->id;
        character->partner.name = std::string(species->name);
        character->partner.stats = proto::computeStats(*species, character->level, {}, {});
    }
    return PartyResult::Ok;
}

DeleteResult DevStore::remove(std::uint64_t accountId, std::uint64_t characterId,
                              std::string_view confirmNickname) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto owned = characters_.find(accountId);
    if (owned == characters_.end()) {
        return DeleteResult::NotFound;
    }
    const auto found = std::find_if(owned->second.begin(), owned->second.end(),
                                    [characterId](const Character& c) {
                                        return c.id == characterId;
                                    });
    if (found == owned->second.end()) {
        return DeleteResult::NotFound;
    }
    if (found->nickname != confirmNickname) {
        return DeleteResult::NameMismatch;
    }
    // 개발용이라 되살릴 일이 없다. 그냥 지운다.
    unlocks_.erase(characterId);
    owned->second.erase(found);
    return DeleteResult::Deleted;
}

DeleteResult DevStore::releasePartner(std::uint64_t accountId, std::uint64_t characterId) {
    std::lock_guard<std::mutex> lock(mutex_);
    Character* character = findLocked(accountId, characterId);
    if (character == nullptr) {
        return DeleteResult::NotFound;
    }
    if (!character->hasPartner) {
        return DeleteResult::Nothing;
    }
    // 해금 목록은 그대로 둔다. 도로 넣는 것이지 잃는 것이 아니다.
    character->hasPartner = false;
    character->partner = {};
    return DeleteResult::Deleted;
}

}  // namespace heaven::data
