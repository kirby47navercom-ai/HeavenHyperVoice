#pragma once

// 파티 화면 갱신. 필드와 인스턴스가 글자 하나까지 같은 응답을 보낸다.
//
// 저장소가 곧 권위다. 방금 쓴 값을 다시 읽어 보내면 부분 적용이나 거절을
// 클라가 따로 해석할 필요가 없다.
//
// characters 가 널이면 (dev 모드) 빈 파티를 보낸다. 파티는 DB 에만 있어서
// 읽을 곳이 없고, 응답 자체를 거르면 클라가 계속 기다린다.

#include <cstdint>
#include <string_view>
#include <vector>

#include "CharacterStore.h"
#include "FieldCodec.h"
#include "PokemonSpecies.h"
#include "TlsSession.h"

namespace heaven::fieldshared {

inline void sendPartyState(data::CharacterStore* characters, net::TlsSession& session,
                           std::uint64_t accountId, std::uint64_t characterId, bool ok,
                           std::string_view message) {
    std::vector<std::uint16_t> party;
    std::vector<std::uint16_t> unlocked;
    std::uint16_t activeDex = 0;

    if (characters != nullptr) {
        if (const auto character = characters->find(accountId, characterId)) {
            party = character->party;
            unlocked = character->unlocked;
            if (character->hasPartner) {
                activeDex = proto::dexOf(character->partner.speciesId);
            }
        }
    }
    session.send(proto::encodePartyState(ok, message, party, activeDex, unlocked));
}

}  // namespace heaven::fieldshared
