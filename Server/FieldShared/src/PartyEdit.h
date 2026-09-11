#pragma once

// SetParty 처리. 필드와 인스턴스가 같은 요청을 받고 같은 검증을 한다.
//
// 두 서버가 다른 것은 "어느 월드에 반영하는가" 한 줄뿐이라 그것만 콜백으로
// 받는다. World 타입이 서버마다 달라서 여기서는 이름조차 알 수 없다.

#include <cstdint>
#include <functional>
#include <vector>

#include <spdlog/spdlog.h>

#include "CharacterStore.h"
#include "FieldCodec.h"
#include "PartyState.h"
#include "PokemonSpecies.h"
#include "TlsSession.h"
#include "WorkQueue.h"

namespace heaven::fieldshared {

// applyPartner 는 저장이 성공했을 때 DB 스레드에서 불린다. 인자는 꺼내 놓을
// 종족의 **내부 번호**이고, 아무도 안 꺼내면 0 이다.
//
// 언제나 true 를 돌려준다. 잘못된 요청은 사유를 실어 보내고 연결은 유지한다 —
// 파티 편집 실패로 접속을 끊을 이유가 없다.
inline bool setParty(net::TlsSession& session, data::CharacterStore* characters,
                     net::WorkQueue& dbQueue, std::uint64_t accountId,
                     std::uint64_t characterId, const HeavenField::SetParty& request,
                     std::function<void(std::uint16_t speciesId)> applyPartner) {
    // dev 모드에는 저장소가 없다. 파티는 DB 에만 있으므로 바꿀 것도 읽을 것도 없다.
    if (characters == nullptr) {
        session.send(proto::encodePartyState(false, "이 서버에서는 파티를 바꿀 수 없습니다", {},
                                             0, {}));
        return true;
    }

    std::vector<std::uint16_t> dexNumbers;
    if (const auto* numbers = request.dex_numbers()) {
        // 상한을 먼저 본다. 저장소도 거르지만 그전에 임의 길이 벡터를 만들지 않는다.
        if (numbers->size() > data::kMaxPartySize) {
            session.send(proto::encodePartyState(false, "파티는 3마리까지입니다", {}, 0, {}));
            return true;
        }
        dexNumbers.reserve(numbers->size());
        for (const std::uint16_t dex : *numbers) {
            dexNumbers.push_back(dex);
        }
    }

    auto self = session.shared_from_this();
    const std::uint16_t activeDex = request.active_dex();

    // DB 왕복이라 IOCP 워커에서 하지 않는다.
    const bool queued = dbQueue.submit(
        [self, characters, accountId, characterId, dexNumbers, activeDex,
         applyPartner = std::move(applyPartner)] {
            const data::PartyResult result =
                characters->setParty(accountId, characterId, dexNumbers, activeDex);
            const bool ok = result == data::PartyResult::Ok;

            sendPartyState(characters, *self, accountId, characterId, ok,
                           data::partyMessage(result));
            if (!ok) {
                return;
            }

            // 월드에도 반영해야 남들 화면의 파트너가 바뀐다. 월드는 내부 번호로
            // 들고 있으므로 도감번호를 되돌린다.
            const proto::SpeciesBase* species =
                activeDex == 0 ? nullptr : proto::findSpeciesByDex(activeDex);
            applyPartner(species != nullptr ? species->id : std::uint16_t{0});

            spdlog::info("party updated: character {} ({} members, active dex {})", characterId,
                         dexNumbers.size(), activeDex);
        });

    if (!queued) {
        spdlog::warn("{}: db queue full, refusing SetParty", session.peer());
        session.send(proto::encodePartyState(false, "서버가 혼잡합니다", {}, 0, {}));
    }
    return true;
}

}  // namespace heaven::fieldshared
