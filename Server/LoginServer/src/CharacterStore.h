#pragma once

// 캐릭터와 종속 포켓몬 조회 인터페이스.
//
// AccountStore 와 나눠 둔 이유는 나중에 이 둘의 주인이 달라지기 때문이다.
// 필드 서버는 캐릭터를 읽어야 하지만 password_hash 를 볼 이유가 없다.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "LoginCodec.h"

namespace heaven::login {

// 저장소가 읽어온 모양 그대로 와이어에 실린다. 중간 변환 구조체를 두면
// 필드 하나 추가할 때 세 곳을 고쳐야 한다.
// slot 0 이 따라다니는 개체다. 여러 마리가 되면 partner 를 vector 로 바꾼다.
using Character = proto::CharacterInfo;
using Partner = proto::PartnerInfo;

enum class CreateCharacterResult {
    Created,
    NicknameTaken,
    SlotsFull,
    UnknownSpecies,
    NotSupported,
    Error,
};

inline const char* describe(CreateCharacterResult result) {
    switch (result) {
        case CreateCharacterResult::Created:        return "created";
        case CreateCharacterResult::NicknameTaken:  return "nickname already taken";
        case CreateCharacterResult::SlotsFull:      return "no free character slot";
        case CreateCharacterResult::UnknownSpecies: return "unknown starter species";
        case CreateCharacterResult::NotSupported:   return "character creation is not available";
        case CreateCharacterResult::Error:          return "internal error";
    }
    return "unknown result";
}

// 계정당 캐릭터 수 상한. 없으면 무한 생성이 된다.
inline constexpr std::size_t kMaxCharactersPerAccount = 3;

class CharacterStore {
public:
    virtual ~CharacterStore() = default;

    virtual std::vector<Character> listByAccount(std::uint64_t accountId) = 0;

    // 조회를 accountId 로 좁힌다. 남의 캐릭터면 결과가 비므로 소유 확인이
    // 조회 자체에 포함된다. 클라이언트가 보낸 characterId 는 절대 단독으로 믿지 않는다.
    virtual std::optional<Character> find(std::uint64_t accountId,
                                          std::uint64_t characterId) = 0;

    // 캐릭터와 파트너를 함께 만든다. 둘 중 하나만 생기는 상태는 없어야 한다.
    virtual CreateCharacterResult create(std::uint64_t accountId, std::string_view nickname,
                                         std::uint16_t speciesId) = 0;

    // 마지막 플레이 시각. 실패해도 입장은 막지 않는다.
    virtual void touchPlayed(std::uint64_t characterId) = 0;

    virtual bool supportsCreation() const = 0;
};

}  // namespace heaven::login
