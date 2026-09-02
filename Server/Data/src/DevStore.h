#pragma once

#include <map>
#include <mutex>
#include <set>

#include "AccountStore.h"
#include "CharacterStore.h"

namespace heaven::data {

// 개발용 임시 구현. **자격증명을 실제로 검증하지 않는다.**
//
// 비어 있지 않은 아이디면 통과시키고, 아이디에서 결정적으로 계정 ID 를 만든다.
// 같은 아이디로 다시 로그인하면 같은 계정 ID 가 나온다.
//
// 캐릭터는 메모리에만 있고 프로세스가 죽으면 사라진다. DB 없이 화면 흐름만
// 볼 때 쓴다 (--account-store dev). 기동 시 경고를 남긴다.
class DevStore : public AccountStore, public CharacterStore {
public:
    // --- AccountStore ---
    std::optional<Account> authenticate(std::string_view username,
                                        std::string_view password) override;

    // 저장할 곳이 없다. 어차피 아무 아이디나 로그인되므로 가입도 의미가 없다.
    CreateAccountResult createAccount(std::string_view username,
                                      const std::string& passwordHash) override;

    const char* describe() const override { return "DevStore (no real authentication)"; }
    bool supportsRegistration() const override { return false; }

    // --- CharacterStore ---
    std::vector<Character> listByAccount(std::uint64_t accountId) override;
    std::optional<Character> find(std::uint64_t accountId, std::uint64_t characterId) override;
    CreateCharacterResult create(std::uint64_t accountId, std::string_view nickname,
                                 std::uint16_t speciesId,
                                 const Appearance& appearance) override;
    PartyResult setParty(std::uint64_t accountId, std::uint64_t characterId,
                         const std::vector<std::uint16_t>& dexNumbers,
                         std::uint16_t activeDex) override;
    DeleteResult remove(std::uint64_t accountId, std::uint64_t characterId,
                        std::string_view confirmNickname) override;
    DeleteResult releasePartner(std::uint64_t accountId, std::uint64_t characterId) override;
    void touchPlayed(std::uint64_t) override {}

    // 개발용이라 위치를 들고 있지 않다. 항상 스폰 지점에서 시작한다.
    std::optional<Position> loadPosition(std::uint64_t) override { return std::nullopt; }
    void savePosition(std::uint64_t, const Position&) override {}
    bool supportsCreation() const override { return true; }

private:
    std::mutex mutex_;
    std::map<std::uint64_t, std::vector<Character>> characters_;

    // 캐릭터 id -> 해금한 종족. 실제 저장소는 도감번호 비트맵이지만 여기서는
    // 집합이면 충분하다.
    std::map<std::uint64_t, std::set<std::uint16_t>> unlocks_;
    std::uint64_t nextCharacterId_ = 1;

    // 잠금을 이미 쥔 쪽에서 부른다.
    Character* findLocked(std::uint64_t accountId, std::uint64_t characterId);
};

}  // namespace heaven::data
