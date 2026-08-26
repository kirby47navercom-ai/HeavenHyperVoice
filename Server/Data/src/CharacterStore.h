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

namespace heaven::data {

// 저장소가 읽어온 모양 그대로 와이어에 실린다. 중간 변환 구조체를 두면
// 필드 하나 추가할 때 세 곳을 고쳐야 한다.
// slot 0 이 따라다니는 개체다. 여러 마리가 되면 partner 를 vector 로 바꾼다.
using Character = proto::CharacterInfo;
using Partner = proto::PartnerInfo;
using Appearance = proto::AppearanceInfo;

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

// 캐릭터 삭제와 파트너 방생이 같은 결과 타입을 쓴다. 실패 사유가 같기 때문이다.
enum class DeleteResult {
    Deleted,
    NotFound,      // 없거나, 남의 것이거나, 이미 지워졌다
    NameMismatch,  // 확인용으로 입력한 닉네임이 다르다
    Nothing,       // 방생할 파트너가 애초에 없다
    NotSupported,
    Error,
};

inline const char* describe(DeleteResult result) {
    switch (result) {
        case DeleteResult::Deleted:      return "deleted";
        case DeleteResult::NotFound:     return "not found";
        case DeleteResult::NameMismatch: return "confirmation name did not match";
        case DeleteResult::Nothing:      return "nothing to release";
        case DeleteResult::NotSupported: return "deletion is not available";
        case DeleteResult::Error:        return "internal error";
    }
    return "unknown result";
}

// 계정당 캐릭터 수 상한. 없으면 무한 생성이 된다.
inline constexpr std::size_t kMaxCharactersPerAccount = 3;

// 마지막으로 저장된 위치. 실시간 위치는 여기 없다 — 필드 서버 메모리와
// Redis 에 있고, 이건 입장할 때 읽고 퇴장할 때 쓰는 값이다.
//
// 높이는 아직 없다. 넣을 때 z 를 추가하고 스키마에 컬럼을 하나 더 만든다.
struct Position {
    std::uint32_t mapId = 0;
    float x = 0.f;
    float y = 0.f;
    float facing = 0.f;
};

class CharacterStore {
public:
    virtual ~CharacterStore() = default;

    virtual std::vector<Character> listByAccount(std::uint64_t accountId) = 0;

    // 조회를 accountId 로 좁힌다. 남의 캐릭터면 결과가 비므로 소유 확인이
    // 조회 자체에 포함된다. 클라이언트가 보낸 characterId 는 절대 단독으로 믿지 않는다.
    virtual std::optional<Character> find(std::uint64_t accountId,
                                          std::uint64_t characterId) = 0;

    // 캐릭터와 파트너를 함께 만든다. 둘 중 하나만 생기는 상태는 없어야 한다.
    //
    // 외형은 호출자가 이미 sanitizeAppearance 를 통과시킨 값이어야 한다.
    virtual CreateCharacterResult create(std::uint64_t accountId, std::string_view nickname,
                                         std::uint16_t speciesId,
                                         const Appearance& appearance) = 0;

    // 소프트 삭제. 행은 남고 deleted_at 이 채워지며, 이후 조회에서 빠진다.
    // 닉네임은 계속 점유된다 (사칭 방지).
    //
    // confirmNickname 은 클라이언트가 사용자에게 다시 입력받은 값이다.
    // characterId 만 믿으면 잘못 누른 한 번으로 다른 캐릭터가 지워진다.
    virtual DeleteResult remove(std::uint64_t accountId, std::uint64_t characterId,
                                std::string_view confirmNickname) = 0;

    // 파트너 방생. 이쪽은 행을 진짜 지운다 — 유령 행이 남으면
    // uq_character_slot 때문에 새 파트너를 slot 0 에 넣을 수 없다.
    virtual DeleteResult releasePartner(std::uint64_t accountId,
                                        std::uint64_t characterId) = 0;

    // 마지막 플레이 시각. 실패해도 입장은 막지 않는다.
    virtual void touchPlayed(std::uint64_t characterId) = 0;

    // 필드 입장/퇴장에서 쓴다. 없는 캐릭터면 nullopt.
    virtual std::optional<Position> loadPosition(std::uint64_t characterId) = 0;
    virtual void savePosition(std::uint64_t characterId, const Position& position) = 0;

    virtual bool supportsCreation() const = 0;
};

}  // namespace heaven::data
