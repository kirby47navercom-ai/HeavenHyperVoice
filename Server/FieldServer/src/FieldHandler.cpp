#include "FieldHandler.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <sstream>

namespace heaven::field {

namespace {

std::string positionKey(std::uint64_t characterId) {
    return "pos:" + std::to_string(characterId);
}

// "map|x|y|facing". 필드 서버끼리만 읽으므로 형식을 단순하게 둔다.
// facing 은 없어도 받아준다 (0 으로 둔다).
std::optional<data::Position> readRedisPosition(net::RedisClient& redis,
                                                std::uint64_t characterId) {
    const auto raw = redis.commandForString({"GET", positionKey(characterId)});
    if (!raw.has_value()) {
        return std::nullopt;
    }

    // 구분자만 공백으로 바꾸면 스트림 추출이 그대로 파서가 된다
    // (MapCollision::loadFromFile 과 같은 방식).
    std::string text = *raw;
    std::replace(text.begin(), text.end(), '|', ' ');

    std::istringstream stream(text);
    data::Position position;
    if (!(stream >> position.mapId >> position.x >> position.y)) {
        return std::nullopt;
    }
    stream >> position.facing;
    return position;
}

}  // namespace

void writeRedisPosition(net::RedisClient& redis, std::uint64_t characterId,
                        const data::Position& position) {
    // TTL 은 세션 등록보다 넉넉하게. 붙어 있는 동안 계속 갱신되고, 끊기면
    // 어차피 DB 에 저장된 뒤라 남아 있어도 해가 없다.
    redis.command({"SET", positionKey(characterId),
                   std::to_string(position.mapId) + "|" + std::to_string(position.x) + "|" +
                       std::to_string(position.y) + "|" + std::to_string(position.facing),
                   "EX", "300"});
}

void clearRedisPosition(net::RedisClient& redis, std::uint64_t characterId) {
    redis.command({"DEL", positionKey(characterId)});
}

bool FieldHandler::onFrame(TlsSession& session, const proto::Bytes& body) {
    Stage stage = Stage::Done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stage = stage_;
    }
    if (stage == Stage::Done) {
        return false;
    }

    const auto* envelope = proto::verifyFieldEnvelope(body);
    if (envelope == nullptr) {
        spdlog::warn("{}: malformed field frame", session.peer());
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
            // 입장 중에 온 Move 는 버린다. 아직 월드에 없다.
            if (stage == Stage::InField) {
                handleMove(*envelope->payload_as_Move());
            }
            return true;

        default:
            spdlog::warn("{}: unexpected field payload", session.peer());
            return false;
    }
}

// 로그인 서버 없이 필드만 붙여볼 때. 티켓도 DB 도 건너뛴다.
bool FieldHandler::enterWithoutAuth(TlsSession& session, const HeavenField::Enter& request) {
    const auto* name = request.dev_name();
    if (name == nullptr || name->size() == 0 || name->size() > proto::kMaxNicknameBytes) {
        session.send(proto::encodeFieldNotice("dev_name 이 필요합니다"));
        return false;
    }

    // 클라가 준 번호를 그대로 쓴다. 개발 모드라 소유 개념이 없다.
    const std::uint64_t characterId = request.dev_character_id();
    if (characterId == 0) {
        session.send(proto::encodeFieldNotice("dev_character_id 는 0 이 될 수 없습니다"));
        return false;
    }

    session.markAuthenticated();

    const data::Position start{0, proto::kSpawnX, proto::kSpawnY, 0.f};
    auto self = session.shared_from_this();

    Displaced displaced;
    {
        // stage_ 를 InField 로 올린 순간부터 onClosed 가 leave() 를 부를 수 있다.
        // 그 사이에 enter() 가 아직 안 끝났으면 leave 가 헛돌고, 뒤늦게 들어간
        // 엔티티는 아무도 지우지 않는 유령이 된다. 둘을 한 락 안에서 한다.
        // 락 순서는 handler -> world 로 일관되고 반대 방향 경로는 없다.
        std::lock_guard<std::mutex> lock(mutex_);
        characterId_ = characterId;
        accountId_ = characterId;  // 계정 개념이 없으므로 같은 값으로 둔다
        nickname_ = name->str();
        stage_ = Stage::InField;

        // EnterAck 이 Spawn 보다 먼저 나가야 한다.
        self->send(
            proto::encodeEnterAck(characterId, start.x, start.y, start.facing, start.mapId));

        displaced = context_.world->enter(characterId, characterId, nickname_,
                                          request.dev_partner_species(), start, self);
    }
    if (displaced.session) {
        displaced.session->send(
            proto::encodeFieldNotice("다른 곳에서 접속하여 연결을 종료합니다"));
        displaced.session->closeAfterFlush();
    }
    // 밀려난 쪽의 위치는 저장하지 않는다. 개발 모드에는 저장소가 아예 없다.

    spdlog::warn("entered WITHOUT AUTH: {} (id {}, {}) - {} in field", nickname_, characterId,
                 session.peer(), context_.world->size());
    return true;
}

bool FieldHandler::handleEnter(TlsSession& session, const HeavenField::Enter& request) {
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
    const proto::TicketError error =
        proto::verifyTicket(ticket, proto::kAudienceField, *context_.keys, proto::nowUnix(), verified);

    if (error != proto::TicketError::Ok) {
        spdlog::warn("{}: ticket rejected - {}", session.peer(), proto::describe(error));
        session.send(proto::encodeFieldNotice(std::string("인증 실패: ") +
                                              proto::describe(error)));
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

    // 캐릭터와 위치를 읽는다. DB 왕복이라 IOCP 워커에서 하면 안 된다.
    auto self = session.shared_from_this();
    const FieldContext* context = &context_;
    FieldHandler* handler = this;
    const std::uint64_t characterId = verified.characterId;
    const std::uint64_t accountId = verified.accountId;

    const bool queued = context_.dbQueue->submit([self, context, handler, characterId,
                                                  accountId] {
        const auto character = context->characters->find(accountId, characterId);
        if (!character.has_value()) {
            spdlog::warn("character {} not found for account {}", characterId, accountId);
            self->send(proto::encodeFieldNotice("캐릭터를 찾을 수 없습니다"));
            self->closeAfterFlush();
            return;
        }

        // Redis 가 더 최신이다. 서버가 비정상 종료하면 DB 에는 반영되지 않는다.
        std::optional<data::Position> position;
        if (context->redis != nullptr) {
            position = readRedisPosition(*context->redis, characterId);
        }
        if (!position.has_value()) {
            position = context->characters->loadPosition(characterId);
        }

        // 컬럼 기본값이 0 이라 한 번도 필드에 들어온 적 없는 캐릭터는 (0,0) 으로
        // 읽힌다. 그건 월드 모서리지 시작 지점이 아니다. 정확히 원점이면
        // 미설정으로 본다 — 실제로 거기 서 있을 일은 없다.
        data::Position start{0, proto::kSpawnX, proto::kSpawnY, 0.f};
        if (position.has_value() && (position->x != 0.f || position->y != 0.f)) {
            start = *position;
        }

        const std::uint16_t partner =
            character->hasPartner ? character->partner.speciesId : std::uint16_t{0};

        Displaced displaced;
        {
            // 로드하는 동안 연결이 끊겼으면 월드에 넣지 않는다.
            //
            // stage_ 를 올리고 락을 놓았다가 enter() 를 부르면, 그 틈에 onClosed
            // 가 leave() 를 헛돌고 뒤늦게 들어간 엔티티가 영구히 남는다.
            // 둘을 한 락 안에서 한다 (락 순서 handler -> world, 역방향 없음).
            std::lock_guard<std::mutex> lock(handler->mutex_);
            if (handler->stage_ != Stage::Entering) {
                return;
            }
            handler->stage_ = Stage::InField;

            // EnterAck 이 Spawn 보다 먼저 나가야 한다. 클라가 자기 번호를 알기 전에
            // 남의 Spawn 을 받으면 어느 것이 자기인지 모른다.
            self->send(proto::encodeEnterAck(characterId, start.x, start.y, start.facing,
                                             start.mapId));

            displaced = context->world->enter(characterId, accountId, character->nickname,
                                              partner, start, self);
        }
        if (displaced.session) {
            displaced.session->send(
                proto::encodeFieldNotice("다른 곳에서 접속하여 연결을 종료합니다"));
            displaced.session->closeAfterFlush();
        }

        // 밀려난 쪽은 leave() 를 타지 못한다 (그 자리는 이미 이 세션 것이다).
        // 여기서 저장하지 않으면 그 캐릭터가 접속 이후 움직인 것이 사라진다.
        // 이미 DB 스레드 위라 그대로 쓴다.
        if (displaced.characterId != 0) {
            context->characters->savePosition(displaced.characterId, displaced.position);
            if (context->redis != nullptr) {
                clearRedisPosition(*context->redis, displaced.characterId);
            }
        }

        spdlog::info("entered: {} (character {}, {}) at ({:.0f}, {:.0f}) - {} in field",
                     character->nickname, characterId, self->peer(), start.x, start.y,
                     context->world->size());
    });

    if (!queued) {
        spdlog::warn("{}: db queue full, refusing entry", session.peer());
        session.send(proto::encodeFieldNotice("서버가 혼잡합니다. 잠시 후 다시 시도해 주세요"));
        return false;
    }
    return true;
}

void FieldHandler::handleMove(const HeavenField::Move& request) {
    context_.world->move(characterId_, request.x(), request.y(), request.facing(),
                         request.sequence());
}

void FieldHandler::onClosed(TlsSession& session) {
    Stage previous = Stage::Done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        previous = stage_;
        stage_ = Stage::Done;
    }
    if (previous != Stage::InField) {
        return;
    }

    // 세션을 함께 넘긴다. 같은 캐릭터로 이미 재접속했다면 월드의 그 자리는
    // 새 세션 것이고, 여기서 지우면 살아 있는 쪽을 떼어내게 된다.
    const auto last = context_.world->leave(characterId_, &session);
    if (!last.has_value()) {
        return;
    }

    spdlog::info("left: {} ({}) - {} in field", nickname_, session.peer(),
                 context_.world->size());

    // 개발 모드에는 저장소가 없다. 위치는 프로세스와 함께 사라진다.
    if (context_.devNoAuth) {
        return;
    }

    // DB 쓰기라 IOCP 워커에서 하지 않는다.
    const FieldContext* context = &context_;
    const std::uint64_t characterId = characterId_;
    const data::Position position = *last;

    context_.dbQueue->submit([context, characterId, position] {
        context->characters->savePosition(characterId, position);
        if (context->redis != nullptr) {
            clearRedisPosition(*context->redis, characterId);
        }
    });
}

}  // namespace heaven::field
