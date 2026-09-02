#include "InstanceHandler.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <vector>

#include "InstanceGeometry.h"

namespace heaven::instance {

bool InstanceHandler::onFrame(TlsSession& session, const proto::Bytes& body) {
    // 단계와 방을 한 락 안에서 함께 집어온다. 나눠 읽으면 그 사이에 onClosed
    // 가 끼어들어 방이 사라진 채로 프레임을 처리하게 된다.
    Stage stage = Stage::Done;
    Room* room = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stage = stage_;
        room = room_;
    }
    if (stage == Stage::Done) {
        return false;
    }

    const auto* envelope = proto::verifyFieldEnvelope(body);
    if (envelope == nullptr) {
        spdlog::warn("{}: malformed instance frame", session.peer());
        return false;
    }

    switch (envelope->payload_type()) {
        case HeavenField::Payload::Enter:
            if (stage != Stage::AwaitingEnter) {
                spdlog::warn("{}: duplicate Enter", session.peer());
                return false;
            }
            return handleEnter(session, *envelope->payload_as_Enter());

        case HeavenField::Payload::Move:
            // 입장 중에 온 Move 는 버린다. 아직 방에 없다.
            if (stage == Stage::InRoom && room != nullptr) {
                handleMove(*room, *envelope->payload_as_Move());
            }
            return true;

        case HeavenField::Payload::SetParty:
            if (stage != Stage::InRoom || room == nullptr) {
                spdlog::warn("{}: SetParty before entering a room", session.peer());
                return false;
            }
            return handleSetParty(session, *room, *envelope->payload_as_SetParty());

        default:
            spdlog::warn("{}: unexpected instance payload", session.peer());
            return false;
    }
}

// 저장소가 곧 권위다. 방금 쓴 값을 다시 읽어 보내면 부분 적용이나 거절을
// 클라가 따로 해석할 필요가 없다.
void InstanceHandler::sendPartyState(const InstanceContext& context, TlsSession& session,
                                     std::uint64_t accountId, std::uint64_t characterId,
                                     bool ok, std::string_view message) {
    std::vector<std::uint16_t> party;
    std::vector<std::uint16_t> unlocked;
    std::uint16_t activeDex = 0;

    if (context.characters != nullptr) {
        if (const auto character = context.characters->find(accountId, characterId)) {
            party = character->party;
            unlocked = character->unlocked;
            if (character->hasPartner) {
                activeDex = proto::dexOf(character->partner.speciesId);
            }
        }
    }
    session.send(proto::encodePartyState(ok, message, party, activeDex, unlocked));
}

// 방 배정과 월드 입장, EnterAck 까지 한 번에 한다.
//
// stage_ 를 InRoom 으로 올린 순간부터 onClosed 가 leave() 를 부를 수 있다.
// 그 사이에 world.enter() 가 아직 안 끝났으면 leave 가 헛돌고, 뒤늦게 들어간
// 엔티티는 아무도 지우지 않는 유령이 된다. 둘을 한 락 안에서 한다.
bool InstanceHandler::placeInRoom(const std::shared_ptr<TlsSession>& self, std::uint32_t type,
                                  std::uint16_t partnerSpecies) {
    Room* room = context_.rooms->join(type);
    if (room == nullptr) {
        // 종류를 모르거나 방 상한에 걸렸다. 둘을 구분해 알려준다.
        const char* reason = context_.rooms->isKnownType(type)
                                 ? "인스턴스가 가득 찼습니다. 잠시 후 다시 시도해 주세요"
                                 : "알 수 없는 인스턴스입니다";
        self->send(proto::encodeFieldNotice(reason));
        return false;
    }

    // 맵이 아직 없어 모두 같은 지점에서 시작한다.
    // ponytail: 겹쳐 선다. 맵이 나오면 스폰 지점 목록에서 골라 쓸 것.
    const data::Position start{type, instance::kSpawnX, instance::kSpawnY, 0.f};

    Displaced displaced;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stage_ == Stage::Done) {
            // 로드하는 동안 연결이 끊겼다. 방에 넣지 않고 자리만 돌려준다.
            context_.rooms->leave(room);
            return false;
        }
        stage_ = Stage::InRoom;
        room_ = room;

        // EnterAck 이 Spawn 보다 먼저 나가야 한다. 클라가 자기 번호를 알기 전에
        // 남의 Spawn 을 받으면 어느 것이 자기인지 모른다.
        self->send(proto::encodeEnterAck(characterId_, start.x, start.y, start.facing,
                                         start.mapId, kWorldOriginOffset, room->id));

        displaced = room->world.enter(characterId_, accountId_, nickname_, partnerSpecies,
                                      start, self);
    }

    if (displaced.session) {
        displaced.session->send(
            proto::encodeFieldNotice("다른 곳에서 접속하여 연결을 종료합니다"));
        displaced.session->closeAfterFlush();
    }
    // 밀려난 쪽의 위치는 저장하지 않는다. 인스턴스는 위치를 안 남긴다.

    spdlog::info("entered: {} (character {}, {}) room {} type {} - {}", nickname_, characterId_,
                 self->peer(), room->id, type, context_.rooms->describe());
    return true;
}

// 로그인 서버 없이 인스턴스만 붙여볼 때. 티켓도 DB 도 건너뛴다.
bool InstanceHandler::enterWithoutAuth(TlsSession& session, const HeavenField::Enter& request) {
    const auto* name = request.dev_name();
    if (name == nullptr || name->size() == 0 || name->size() > proto::kMaxNicknameBytes) {
        session.send(proto::encodeFieldNotice("dev_name 이 필요합니다"));
        return false;
    }

    const std::uint64_t characterId = request.dev_character_id();
    if (characterId == 0) {
        session.send(proto::encodeFieldNotice("dev_character_id 는 0 이 될 수 없습니다"));
        return false;
    }

    session.markAuthenticated();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        characterId_ = characterId;
        accountId_ = characterId;  // 계정 개념이 없으므로 같은 값으로 둔다
        nickname_ = name->str();
    }

    auto self = session.shared_from_this();
    if (!placeInRoom(self, request.instance_type(), request.dev_partner_species())) {
        return false;
    }

    spdlog::warn("^ WITHOUT AUTH");
    return true;
}

bool InstanceHandler::handleEnter(TlsSession& session, const HeavenField::Enter& request) {
    if (context_.devNoAuth) {
        return enterWithoutAuth(session, request);
    }

    const auto* blob = request.ticket();
    if (blob == nullptr || blob->size() == 0) {
        session.send(proto::encodeFieldNotice("입장권이 없습니다"));
        return false;
    }

    const proto::Bytes ticket(blob->begin(), blob->end());
    proto::VerifiedTicket verified;
    const proto::TicketError error = proto::verifyTicket(
        ticket, proto::kAudienceInstance, *context_.keys, proto::nowUnix(), verified);

    if (error != proto::TicketError::Ok) {
        spdlog::warn("{}: ticket rejected - {}", session.peer(), proto::describe(error));
        session.send(
            proto::encodeFieldNotice(std::string("인증 실패: ") + proto::describe(error)));
        return false;
    }

    const std::uint32_t type = request.instance_type();
    if (!context_.rooms->isKnownType(type)) {
        // 방을 만들기 전에 거른다. 클라이언트가 보낸 번호를 그대로 믿고 방을
        // 만들면 아무 숫자나 밀어 넣는 것만으로 방이 무한히 생긴다.
        spdlog::warn("{}: unknown instance type {}", session.peer(), type);
        session.send(proto::encodeFieldNotice("알 수 없는 인스턴스입니다"));
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        characterId_ = verified.characterId;
        accountId_ = verified.accountId;
        nickname_ = verified.nickname;
        stage_ = Stage::Entering;
    }
    session.markAuthenticated();

    // 파트너와 파티를 읽는다. DB 왕복이라 IOCP 워커에서 하면 안 된다.
    auto self = session.shared_from_this();
    const InstanceContext* context = &context_;
    InstanceHandler* handler = this;
    const std::uint64_t characterId = verified.characterId;
    const std::uint64_t accountId = verified.accountId;

    const bool queued = context_.dbQueue->submit([self, context, handler, characterId, accountId,
                                                  type] {
        const auto character = context->characters->find(accountId, characterId);
        if (!character.has_value()) {
            spdlog::warn("character {} not found for account {}", characterId, accountId);
            self->send(proto::encodeFieldNotice("캐릭터를 찾을 수 없습니다"));
            self->closeAfterFlush();
            return;
        }

        const std::uint16_t partner =
            character->hasPartner ? character->partner.speciesId : std::uint16_t{0};

        if (!handler->placeInRoom(self, type, partner)) {
            self->closeAfterFlush();
            return;
        }

        // 파티 화면이 열릴 때 조회하지 않도록 입장할 때 한 번 실어 보낸다.
        // 이미 DB 스레드 위라 그대로 쓴다.
        sendPartyState(*context, *self, accountId, characterId, true, "");
    });

    if (!queued) {
        spdlog::warn("{}: db queue full, refusing entry", session.peer());
        session.send(proto::encodeFieldNotice("서버가 혼잡합니다. 잠시 후 다시 시도해 주세요"));
        return false;
    }
    return true;
}

void InstanceHandler::handleMove(Room& room, const HeavenField::Move& request) {
    room.world.move(characterId_, request.x(), request.y(), request.facing(),
                    request.sequence());
}

bool InstanceHandler::handleSetParty(TlsSession& session, Room& room,
                                     const HeavenField::SetParty& request) {
    // dev 모드에는 저장소가 없다. 파티는 DB 에만 있으므로 바꿀 것도 읽을 것도 없다.
    if (context_.characters == nullptr) {
        session.send(proto::encodePartyState(false, "이 서버에서는 파티를 바꿀 수 없습니다", {},
                                             0, {}));
        return true;
    }

    std::vector<std::uint16_t> dexNumbers;
    if (const auto* numbers = request.dex_numbers()) {
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
    const InstanceContext* context = &context_;
    Room* roomPtr = &room;
    const std::uint64_t accountId = accountId_;
    const std::uint64_t characterId = characterId_;
    const std::uint16_t activeDex = request.active_dex();

    const bool queued = context_.dbQueue->submit(
        [self, context, roomPtr, accountId, characterId, dexNumbers, activeDex] {
            const data::PartyResult result =
                context->characters->setParty(accountId, characterId, dexNumbers, activeDex);

            const char* message = nullptr;
            switch (result) {
                case data::PartyResult::Ok:           message = "파티를 저장했습니다"; break;
                case data::PartyResult::NotFound:     message = "캐릭터를 찾을 수 없습니다"; break;
                case data::PartyResult::NotUnlocked:  message = "해금하지 않은 포켓몬입니다"; break;
                case data::PartyResult::TooMany:      message = "파티는 3마리까지입니다"; break;
                case data::PartyResult::Duplicate:    message = "같은 포켓몬을 두 번 넣을 수 없습니다"; break;
                case data::PartyResult::NotInParty:   message = "파티에 없는 포켓몬은 꺼낼 수 없습니다"; break;
                case data::PartyResult::NotSupported: message = "이 서버는 파티 편집을 지원하지 않습니다"; break;
                case data::PartyResult::Error:        message = "서버 오류로 저장하지 못했습니다"; break;
            }

            const bool ok = result == data::PartyResult::Ok;
            sendPartyState(*context, *self, accountId, characterId, ok, message);

            if (ok) {
                // 방에도 반영해야 같은 방 사람들 화면의 파트너가 바뀐다.
                const proto::SpeciesBase* species =
                    activeDex == 0 ? nullptr : proto::findSpeciesByDex(activeDex);
                roomPtr->world.setPartnerSpecies(
                    characterId, species != nullptr ? species->id : std::uint16_t{0});
            }
        });

    if (!queued) {
        spdlog::warn("{}: db queue full, refusing SetParty", session.peer());
        session.send(proto::encodePartyState(false, "서버가 혼잡합니다", {}, 0, {}));
    }
    return true;
}

void InstanceHandler::onClosed(TlsSession& session) {
    Stage previous = Stage::Done;
    Room* room = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        previous = stage_;
        room = room_;
        stage_ = Stage::Done;
        // room_ 은 그대로 둔다 — 헤더의 주석 참고.
    }
    if (previous != Stage::InRoom || room == nullptr) {
        return;
    }

    // 세션을 함께 넘긴다. 같은 캐릭터로 이미 재접속했다면 방의 그 자리는
    // 새 세션 것이고, 여기서 지우면 살아 있는 쪽을 떼어내게 된다.
    room->world.leave(characterId_, &session);

    // 밀려나서 leave 가 헛돌았더라도 자리는 돌려줘야 한다. 이 세션이 join 으로
    // 한 자리를 잡았기 때문이다 — 안 돌려주면 방이 영영 안 빈다.
    context_.rooms->leave(room);

    spdlog::info("left: {} ({}) room {} - {}", nickname_, session.peer(), room->id,
                 context_.rooms->describe());
}

}  // namespace heaven::instance
