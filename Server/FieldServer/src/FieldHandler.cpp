#include "FieldHandler.h"

#include <spdlog/spdlog.h>

#include <charconv>
#include <chrono>
#include <memory>

namespace heaven::field {

namespace {

std::int64_t nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

std::string positionKey(std::uint64_t characterId) {
    return "pos:" + std::to_string(characterId);
}

// "map|x|y|facing". 필드 서버끼리만 읽으므로 형식을 단순하게 둔다.
std::optional<data::Position> readRedisPosition(net::RedisClient& redis,
                                                std::uint64_t characterId) {
    const auto raw = redis.commandForString({"GET", positionKey(characterId)});
    if (!raw.has_value()) {
        return std::nullopt;
    }

    data::Position position;
    const char* cursor = raw->data();
    const char* end = raw->data() + raw->size();
    int index = 0;

    while (cursor < end && index < 4) {
        const char* separator = cursor;
        while (separator < end && *separator != '|') {
            ++separator;
        }
        if (index == 0) {
            std::from_chars(cursor, separator, position.mapId);
        } else {
            float value = 0.f;
            if (std::from_chars(cursor, separator, value).ec != std::errc{}) {
                return std::nullopt;
            }
            if (index == 1) position.x = value;
            if (index == 2) position.y = value;
            if (index == 3) position.facing = value;
        }
        cursor = separator + 1;
        ++index;
    }
    return index >= 3 ? std::optional<data::Position>(position) : std::nullopt;
}

void writeRedisPosition(net::RedisClient& redis, std::uint64_t characterId,
                        const data::Position& position) {
    // TTL 은 세션 등록보다 넉넉하게. 붙어 있는 동안 계속 갱신되고, 끊기면
    // 어차피 DB 에 저장된 뒤라 남아 있어도 해가 없다.
    redis.command({"SET", positionKey(characterId),
                   std::to_string(position.mapId) + "|" + std::to_string(position.x) + "|" +
                       std::to_string(position.y) + "|" + std::to_string(position.facing),
                   "EX", "300"});
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

bool FieldHandler::handleEnter(TlsSession& session, const HeavenField::Enter& request) {
    const auto* blob = request.ticket();
    if (blob == nullptr || blob->size() == 0) {
        session.send(proto::encodeFieldNotice("입장권이 없습니다"));
        return false;
    }

    const proto::Bytes ticket(blob->begin(), blob->end());
    proto::VerifiedTicket verified;
    const proto::TicketError error =
        proto::verifyTicket(ticket, proto::kAudienceField, *context_.keys, nowUnix(), verified);

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

    context_.dbQueue->submit([self, context, handler, characterId, accountId] {
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

        // 로드하는 동안 연결이 끊겼으면 월드에 넣지 않는다.
        {
            std::lock_guard<std::mutex> lock(handler->mutex_);
            if (handler->stage_ != Stage::Entering) {
                return;
            }
            handler->stage_ = Stage::InField;
        }

        // EnterAck 이 Spawn 보다 먼저 나가야 한다. 클라가 자기 번호를 알기 전에
        // 남의 Spawn 을 받으면 어느 것이 자기인지 모른다.
        self->send(proto::encodeEnterAck(characterId, start.x, start.y, start.facing,
                                         start.mapId));

        const std::uint16_t partner =
            character->hasPartner ? character->partner.speciesId : std::uint16_t{0};
        const auto displaced = context->world->enter(characterId, accountId,
                                                     character->nickname, partner, start, self);
        if (displaced) {
            displaced->send(proto::encodeFieldNotice("다른 곳에서 접속하여 연결을 종료합니다"));
            displaced->closeAfterFlush();
        }

        spdlog::info("entered: {} (character {}, {}) at ({:.0f}, {:.0f}) - {} in field",
                     character->nickname, characterId, self->peer(), start.x, start.y,
                     context->world->size());
    });

    return true;
}

void FieldHandler::handleMove(const HeavenField::Move& request) {
    context_.world->move(characterId_, request.x(), request.y(), request.facing());
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

    const auto last = context_.world->leave(characterId_);
    if (!last.has_value()) {
        return;
    }

    spdlog::info("left: {} ({}) - {} in field", nickname_, session.peer(),
                 context_.world->size());

    // DB 쓰기라 IOCP 워커에서 하지 않는다.
    const FieldContext* context = &context_;
    const std::uint64_t characterId = characterId_;
    const data::Position position = *last;

    context_.dbQueue->submit([context, characterId, position] {
        context->characters->savePosition(characterId, position);
        if (context->redis != nullptr) {
            context->redis->command({"DEL", positionKey(characterId)});
        }
    });
}

}  // namespace heaven::field
