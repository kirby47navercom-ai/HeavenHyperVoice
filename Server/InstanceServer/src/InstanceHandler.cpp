#include "InstanceHandler.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <vector>

#include "InstanceGeometry.h"
#include "PartyEdit.h"

namespace heaven::instance {

bool InstanceHandler::onFrame(TlsSession &session, const proto::Bytes &body) {
    // 단계와 방을 한 락 안에서 함께 집어온다. 나눠 읽으면 그 사이에 onClosed
    // 가 끼어들어 방이 사라진 채로 프레임을 처리하게 된다.
    Stage stage = Stage::Done;
    Room *room = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stage = stage_;
        room = room_;
    }
    if (stage == Stage::Done) {
        return false;
    }

    const auto *envelope = proto::verifyFieldEnvelope(body);
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
            return handleMove(session, *room, *envelope->payload_as_Move());
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

// 방 배정과 월드 입장, EnterAck 까지 한 번에 한다.
//
// stage_ 를 InRoom 으로 올린 순간부터 onClosed 가 leave() 를 부를 수 있다.
// 그 사이에 world.enter() 가 아직 안 끝났으면 leave 가 헛돌고, 뒤늦게 들어간
// 엔티티는 아무도 지우지 않는 유령이 된다. 둘을 한 락 안에서 한다.
bool InstanceHandler::placeInRoom(const std::shared_ptr<TlsSession> &self, std::uint32_t type,
                                  std::uint16_t partnerSpecies, const proto::AppearanceInfo &appearance,
                                  std::uint64_t partyId) {
    // 파티가 이미 방을 잡아 뒀으면 그 번호를, 아무도 안 잡았으면 0 을 받는다.
    // 실제 확정은 방을 배정받은 뒤에 한다 — 여기서 심으면 배정에 실패했을 때
    // 아무도 못 들어가는 방 번호가 남는다.
    std::uint32_t preferred = 0;
    if (partyId != 0 && context_.party != nullptr) {
        preferred = context_.party->claimRoom(partyId, type, 0);
    }

    Room *room = context_.rooms->join(type, preferred);
    if (room == nullptr) {
        // 종류를 모르거나 방 상한에 걸렸다. 둘을 구분해 알려준다.
        const char *reason = context_.rooms->isKnownType(type)
                                 ? "인스턴스가 가득 찼습니다. 잠시 후 다시 시도해 주세요"
                                 : "알 수 없는 인스턴스입니다";
        self->send(proto::encodeFieldNotice(reason));
        return false;
    }

    // 배정이 끝났으니 이 방을 파티의 방으로 굳힌다. 먼저 들어온 쪽이 이기고,
    // 나중에 온 쪽은 위에서 그 번호를 받아 따라온다.
    if (partyId != 0 && context_.party != nullptr && preferred == 0) {
        context_.party->claimRoom(partyId, type, room->id);
    }

    const data::Position start =
        room->world.resolvePosition(data::Position{type, instance::kSpawnX, instance::kSpawnY, 0.f});

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
        self->send(proto::encodeEnterAck(characterId_, start.x, start.y, start.z, start.facing, start.mapId,
                                         kWorldOriginOffset, room->id, room->world.collisionHash()));

        displaced =
            room->world.enter(characterId_, accountId_, nickname_, partnerSpecies, appearance, start, self);
    }

    if (displaced.session) {
        displaced.session->send(proto::encodeFieldNotice("다른 곳에서 접속하여 연결을 종료합니다"));
        displaced.session->closeAfterFlush();
    }
    // 밀려난 쪽의 위치는 저장하지 않는다. 인스턴스는 위치를 안 남긴다.

    // 채팅 서버가 인스턴스 채팅을 보내려면 누가 어느 방에 있는지 알아야 한다.
    // 실패해도 입장은 막지 않는다 — 채팅 한 채널만 안 될 뿐이다.
    if (context_.presence != nullptr) {
        context_.presence->enter(accountId_, type, room->id);
    }

    spdlog::info("entered: {} (character {}, {}) room {} type {} - {}", nickname_, characterId_, self->peer(),
                 room->id, type, context_.rooms->describe());
    return true;
}

// 로그인 서버 없이 인스턴스만 붙여볼 때. 티켓도 DB 도 건너뛴다.
bool InstanceHandler::enterWithoutAuth(TlsSession &session, const HeavenField::Enter &request) {
    const auto *name = request.dev_name();
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
        accountId_ = characterId; // 계정 개념이 없으므로 같은 값으로 둔다
        nickname_ = name->str();
    }

    auto self = session.shared_from_this();
    // dev 경로에는 DB 가 없다. 외형은 기본값이다.
    if (!placeInRoom(self, request.instance_type(), request.dev_partner_species(), proto::AppearanceInfo{},
                     0)) {
        return false;
    }

    spdlog::warn("^ WITHOUT AUTH");
    return true;
}

bool InstanceHandler::handleEnter(TlsSession &session, const HeavenField::Enter &request) {
    if (request.core_version() != hhv::movement::Version) {
        session.send(proto::encodeFieldNotice("Movement core version mismatch"));
        return false;
    }

    if (context_.devNoAuth) {
        return enterWithoutAuth(session, request);
    }

    const auto *blob = request.ticket();
    if (blob == nullptr || blob->size() == 0) {
        session.send(proto::encodeFieldNotice("입장권이 없습니다"));
        return false;
    }

    const proto::Bytes ticket(blob->begin(), blob->end());
    proto::VerifiedTicket verified;
    const proto::TicketError error =
        proto::verifyTicket(ticket, proto::kAudienceInstance, *context_.keys, proto::nowUnix(), verified);

    if (error != proto::TicketError::Ok) {
        spdlog::warn("{}: ticket rejected - {}", session.peer(), proto::describe(error));
        session.send(proto::encodeFieldNotice(std::string("인증 실패: ") + proto::describe(error)));
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
    const InstanceContext *context = &context_;
    InstanceHandler *handler = this;
    const std::uint64_t characterId = verified.characterId;
    const std::uint64_t accountId = verified.accountId;

    const bool queued = context_.dbQueue->submit([self, context, handler, characterId, accountId, type] {
        // 파티 확인이 DB 조회보다 먼저다. 못 들어갈 사람이면 DB 를 칠 이유가 없다.
        //
        // 클라이언트가 보낸 파티 번호는 쓰지 않는다. 계정으로 되짚어야 남의 파티
        // 번호를 넣어 남의 방에 끼어드는 것을 막을 수 있다.
        std::uint64_t partyId = 0;
        if (context->party != nullptr) {
            partyId = context->party->partyIdOf(accountId);

            // 파티에 속해 있으면 파티장이 연 입장만 유효하다. 클라이언트 포탈에서만
            // 막으면 우회된다 — 개별 입장 금지의 실제 집행 지점이 여기다.
            if (partyId != 0 && !context->party->entryOpen(partyId, type)) {
                self->send(proto::encodeFieldNotice("파티장이 입장을 시작해야 들어갈 수 있습니다"));
                self->closeAfterFlush();
                return;
            }
        }

        const auto character = context->characters->find(accountId, characterId);
        if (!character.has_value()) {
            spdlog::warn("character {} not found for account {}", characterId, accountId);
            self->send(proto::encodeFieldNotice("캐릭터를 찾을 수 없습니다"));
            self->closeAfterFlush();
            return;
        }

        // 파티가 비어 있으면 들어가도 할 수 있는 것이 없다. 야생을 상대할
        // 포켓몬이 한 마리도 없기 때문이다. 클라이언트 포탈에서도 막지만
        // 집행은 여기다 — 포탈을 우회해 붙어도 이 검사는 지나야 한다.
        if (character->party.empty()) {
            self->send(proto::encodeFieldNotice("파티에 포켓몬을 한 마리 이상 넣어야 들어갈 수 있습니다"));
            self->closeAfterFlush();
            return;
        }

        const std::uint16_t partner = character->hasPartner ? character->partner.speciesId : std::uint16_t{0};

        if (!handler->placeInRoom(self, type, partner, character->appearance, partyId)) {
            self->closeAfterFlush();
            return;
        }

        // 파티 화면이 열릴 때 조회하지 않도록 입장할 때 한 번 실어 보낸다.
        // 이미 DB 스레드 위라 그대로 쓴다.
        fieldshared::sendPartyState(context->characters, *self, accountId, characterId, true, "");
    });

    if (!queued) {
        spdlog::warn("{}: db queue full, refusing entry", session.peer());
        session.send(proto::encodeFieldNotice("서버가 혼잡합니다. 잠시 후 다시 시도해 주세요"));
        return false;
    }
    return true;
}

bool InstanceHandler::handleMove(TlsSession &session, Room &room, const HeavenField::Move &request) {
    return room.world.move(characterId_, &session, hhv::movement::wire::decodeInputs(request));
}

bool InstanceHandler::handleSetParty(TlsSession &session, Room &room, const HeavenField::SetParty &request) {
    // 방에 반영해야 같은 방 사람들 화면의 파트너가 바뀐다.
    Room *roomPtr = &room;
    const std::uint64_t characterId = characterId_;
    return fieldshared::setParty(session, context_.characters, *context_.dbQueue, accountId_, characterId,
                                 request, [roomPtr, characterId](std::uint16_t speciesId) {
                                     roomPtr->world.setPartnerSpecies(characterId, speciesId);
                                 });
}

void InstanceHandler::onClosed(TlsSession &session) {
    Stage previous = Stage::Done;
    Room *room = nullptr;
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

    // 방을 나갔다고 알린다. 안 지우면 필드로 돌아간 뒤에도 인스턴스 채팅이 온다.
    if (context_.presence != nullptr) {
        context_.presence->leave(accountId_);
    }

    spdlog::info("left: {} ({}) room {} - {}", nickname_, session.peer(), room->id,
                 context_.rooms->describe());
}

} // namespace heaven::instance
