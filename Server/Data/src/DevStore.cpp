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
                                       std::uint16_t speciesId) {
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
    if (species != nullptr) {
        character.hasPartner = true;
        character.partner.speciesId = speciesId;
        character.partner.level = proto::kStarterLevel;
        // 개발용이라 개체값을 굴리지 않고 만렙으로 둔다. 값이 매번 달라지면
        // 화면을 눈으로 비교하기 어렵다.
        character.partner.ivs = {31, 31, 31, 31, 31, 31};
        character.partner.stats = proto::computeStats(*species, proto::kStarterLevel,
                                                      character.partner.ivs,
                                                      character.partner.evs);
    }
    owned.push_back(std::move(character));

    return CreateCharacterResult::Created;
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
    owned->second.erase(found);
    return DeleteResult::Deleted;
}

DeleteResult DevStore::releasePartner(std::uint64_t accountId, std::uint64_t characterId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto owned = characters_.find(accountId);
    if (owned == characters_.end()) {
        return DeleteResult::NotFound;
    }
    for (Character& character : owned->second) {
        if (character.id != characterId) {
            continue;
        }
        if (!character.hasPartner) {
            return DeleteResult::Nothing;
        }
        character.hasPartner = false;
        character.partner = {};
        return DeleteResult::Deleted;
    }
    return DeleteResult::NotFound;
}

}  // namespace heaven::data
