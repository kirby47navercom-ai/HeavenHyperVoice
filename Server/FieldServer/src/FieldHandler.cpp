#include "FieldHandler.h"

#include "PartyEdit.h"

#include <vector>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <random>
#include <sstream>

namespace heaven::field {

namespace {

std::string positionKey(std::uint64_t characterId) {
    return "pos:" + std::to_string(characterId);
}

// "map|x|y|z|facing". 예전 "map|x|y|facing" 도 받아준다.
// facing 은 없어도 받아준다 (0 으로 둔다).
std::optional<data::Position> readRedisPosition(net::RedisClient &redis, std::uint64_t characterId) {
    const auto raw = redis.commandForString({"GET", positionKey(characterId)});
    if (!raw.has_value()) {
        return std::nullopt;
    }

    // 구분자만 공백으로 바꾸면 스트림 추출이 그대로 파서가 된다
    // (Map::loadFromFile 과 같은 방식).
    std::string text = *raw;
    std::replace(text.begin(), text.end(), '|', ' ');

    std::istringstream stream(text);
    data::Position position;
    if (!(stream >> position.mapId >> position.x >> position.y)) {
        return std::nullopt;
    }
    float first = 0.f;
    if (stream >> first) {
        float second = 0.f;
        if (stream >> second) {
            position.z = first;
            position.facing = second;
        } else {
            position.facing = first;
        }
    }
    return position;
}

} // namespace

void writeRedisPosition(net::RedisClient &redis, std::uint64_t characterId, const data::Position &position) {
    // TTL 은 세션 등록보다 넉넉하게. 붙어 있는 동안 계속 갱신되고, 끊기면
    // 어차피 DB 에 저장된 뒤라 남아 있어도 해가 없다.
    redis.command({"SET", positionKey(characterId),
                   std::to_string(position.mapId) + "|" + std::to_string(position.x) + "|" +
                       std::to_string(position.y) + "|" + std::to_string(position.z) + "|" +
                       std::to_string(position.facing),
                   "EX", "300"});
}

void clearRedisPosition(net::RedisClient &redis, std::uint64_t characterId) {
    redis.command({"DEL", positionKey(characterId)});
}

bool FieldHandler::onFrame(TlsSession &session, const proto::Bytes &body) {
    Stage stage = Stage::Done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stage = stage_;
    }
    if (stage == Stage::Done) {
        return false;
    }

    const auto *envelope = proto::verifyFieldEnvelope(body);
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
            return handleMove(session, *envelope->payload_as_Move());
        }
        return true;

    case HeavenField::Payload::SetParty:
        if (stage != Stage::InField) {
            spdlog::warn("{}: SetParty before entering the field", session.peer());
            return false;
        }
        return handleSetParty(session, *envelope->payload_as_SetParty());

    case HeavenField::Payload::GachaDrawRequest:
        if (stage != Stage::InField) {
            spdlog::warn("{}: gacha draw before entering the field", session.peer());
            return false;
        }
        return handleGachaDraw(session, *envelope->payload_as_GachaDrawRequest());

    case HeavenField::Payload::DebugGrantToken:
        if (stage != Stage::InField) {
            spdlog::warn("{}: debug token before entering the field", session.peer());
            return false;
        }
        return handleDebugGrantToken(session);

    default:
        spdlog::warn("{}: unexpected field payload", session.peer());
        return false;
    }
}

bool FieldHandler::handleSetParty(TlsSession &session, const HeavenField::SetParty &request) {
    World *world = context_.world;
    const std::uint64_t characterId = characterId_;
    return fieldshared::setParty(
        session, context_.characters, *context_.dbQueue, accountId_, characterId, request,
        [world, characterId](std::uint16_t speciesId) { world->setPartnerSpecies(characterId, speciesId); });
}

// 로그인 서버 없이 필드만 붙여볼 때. 티켓도 DB 도 건너뛴다.
bool FieldHandler::enterWithoutAuth(TlsSession &session, const HeavenField::Enter &request) {
    const auto *name = request.dev_name();
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

    const data::Position start =
        context_.world->resolvePosition(data::Position{0, proto::kSpawnX, proto::kSpawnY, 0.f});
    auto self = session.shared_from_this();

    Displaced displaced;
    {
        // stage_ 를 InField 로 올린 순간부터 onClosed 가 leave() 를 부를 수 있다.
        // 그 사이에 enter() 가 아직 안 끝났으면 leave 가 헛돌고, 뒤늦게 들어간
        // 엔티티는 아무도 지우지 않는 유령이 된다. 둘을 한 락 안에서 한다.
        // 락 순서는 handler -> world 로 일관되고 반대 방향 경로는 없다.
        std::lock_guard<std::mutex> lock(mutex_);
        characterId_ = characterId;
        accountId_ = characterId; // 계정 개념이 없으므로 같은 값으로 둔다
        nickname_ = name->str();
        stage_ = Stage::InField;

        // EnterAck 이 Spawn 보다 먼저 나가야 한다.
        self->send(proto::encodeEnterAck(characterId, start.x, start.y, start.z, start.facing, start.mapId,
                                         proto::kWorldSize / 2.f, 0, context_.world->collisionHash()));

        // dev 경로에는 DB 가 없다. 외형은 기본값이다.
        displaced = context_.world->enter(characterId, characterId, nickname_, request.dev_partner_species(),
                                          proto::AppearanceInfo{}, start, self);
    }
    if (displaced.session) {
        displaced.session->send(proto::encodeFieldNotice("다른 곳에서 접속하여 연결을 종료합니다"));
        displaced.session->closeAfterFlush();
    }
    // 밀려난 쪽의 위치는 저장하지 않는다. 개발 모드에는 저장소가 아예 없다.

    spdlog::warn("entered WITHOUT AUTH: {} (id {}, {}) - {} in field", nickname_, characterId, session.peer(),
                 context_.world->size());
    return true;
}

bool FieldHandler::handleEnter(TlsSession &session, const HeavenField::Enter &request) {
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
        proto::verifyTicket(ticket, proto::kAudienceField, *context_.keys, proto::nowUnix(), verified);

    if (error != proto::TicketError::Ok) {
        spdlog::warn("{}: ticket rejected - {}", session.peer(), proto::describe(error));
        session.send(proto::encodeFieldNotice(std::string("인증 실패: ") + proto::describe(error)));
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
    const FieldContext *context = &context_;
    FieldHandler *handler = this;
    const std::uint64_t characterId = verified.characterId;
    const std::uint64_t accountId = verified.accountId;

    const bool queued = context_.dbQueue->submit([self, context, handler, characterId, accountId] {
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
        data::Position start =
            context->world->resolvePosition(data::Position{0, proto::kSpawnX, proto::kSpawnY, 0.f});
        if (position.has_value() && (position->x != 0.f || position->y != 0.f)) {
            start = context->world->resolvePosition(*position);
        }

        const std::uint16_t partner = character->hasPartner ? character->partner.speciesId : std::uint16_t{0};

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
            self->send(proto::encodeEnterAck(characterId, start.x, start.y, start.z, start.facing,
                                             start.mapId, proto::kWorldSize / 2.f, 0,
                                             context->world->collisionHash()));

            displaced = context->world->enter(characterId, accountId, character->nickname, partner,
                                              character->appearance, start, self);
        }
        if (displaced.session) {
            displaced.session->send(proto::encodeFieldNotice("다른 곳에서 접속하여 연결을 종료합니다"));
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

        // 파티 화면이 열릴 때 조회하지 않도록 입장할 때 한 번 실어 보낸다.
        // 이미 DB 스레드 위라 그대로 쓴다.
        fieldshared::sendPartyState(context->characters, *self, accountId, characterId, true, "");

        // 뽑기 화면이 열릴 때 따로 조회하지 않게 토큰도 같이 보낸다.
        std::uint32_t tokens = 0;
        if (context->characters->tokenBalance(accountId, characterId, tokens)) {
            self->send(proto::encodeTokenBalance(tokens));
        }

        spdlog::info("entered: {} (character {}, {}) at ({:.0f}, {:.0f}) - {} in field", character->nickname,
                     characterId, self->peer(), start.x, start.y, context->world->size());
    });

    if (!queued) {
        spdlog::warn("{}: db queue full, refusing entry", session.peer());
        session.send(proto::encodeFieldNotice("서버가 혼잡합니다. 잠시 후 다시 시도해 주세요"));
        return false;
    }
    return true;
}

// ------------------------------------------------------------------ 뽑기

bool FieldHandler::handleGachaDraw(TlsSession &session,
                                   const HeavenField::GachaDrawRequest &request) {
    // dev 모드에는 저장소가 없다. 해금을 남길 곳이 없으니 뽑을 것도 없다.
    if (context_.characters == nullptr || context_.gacha == nullptr ||
        !context_.gacha->loaded()) {
        session.send(proto::encodeGachaResult(false, "이 서버에서는 뽑기를 할 수 없습니다", 0,
                                              proto::GachaRarity::Normal, false));
        return true;
    }
    if (!context_.characters->supportsGacha()) {
        session.send(proto::encodeGachaResult(
            false, "서버가 뽑기를 준비하지 못했습니다", 0, proto::GachaRarity::Normal, false));
        return true;
    }

    const auto wireType = static_cast<std::uint8_t>(request.type());
    if (!proto::isGachaType(wireType)) {
        // 0 은 타입을 안 정한 것이다. 기본값으로 굴려 주지 않는다.
        spdlog::warn("{}: gacha draw with type {}", session.peer(), wireType);
        session.send(proto::encodeGachaResult(false, "뽑기 종류가 올바르지 않습니다", 0,
                                              proto::GachaRarity::Normal, false));
        return true;
    }
    const auto type = static_cast<proto::GachaType>(wireType);

    auto self = session.shared_from_this();
    const FieldContext *context = &context_;
    const std::uint64_t accountId = accountId_;
    const std::uint64_t characterId = characterId_;

    // 추첨과 DB 왕복이라 IOCP 워커에서 하지 않는다.
    const bool queued = context_.dbQueue->submit([self, context, accountId, characterId, type] {
        // mt19937 은 스레드 안전하지 않다. DB 스레드마다 하나씩 든다.
        // 방마다 재현이 필요한 야생 배치와 달리 뽑기는 씨앗을 고정할 이유가 없다.
        thread_local std::mt19937 rng{std::random_device{}()};

        const proto::GachaDraw rolled = context->gacha->draw(type, rng);
        if (rolled.dex == 0) {
            self->send(proto::encodeGachaResult(false, "뽑을 수 있는 포켓몬이 없습니다", 0,
                                                proto::GachaRarity::Normal, false));
            return;
        }

        std::uint32_t tokensLeft = 0;
        const data::GachaResult result =
            context->characters->drawGacha(accountId, characterId, rolled.dex, tokensLeft);

        const char *message = nullptr;
        switch (result) {
            case data::GachaResult::Granted:      message = "새로운 포켓몬을 얻었습니다"; break;
            case data::GachaResult::Duplicate:    message = "이미 가지고 있는 포켓몬입니다"; break;
            case data::GachaResult::NoToken:      message = "포켓몬 토큰이 없습니다"; break;
            case data::GachaResult::NotSupported: message = "이 서버에서는 뽑기를 할 수 없습니다"; break;
            case data::GachaResult::Error:        message = "서버 오류로 뽑지 못했습니다"; break;
        }

        const bool spent = result == data::GachaResult::Granted ||
                           result == data::GachaResult::Duplicate;
        const bool duplicate = result == data::GachaResult::Duplicate;

        // 꽝이어도 무엇이 나왔는지는 알려준다. 숨기면 토큰만 사라진 것으로 보인다.
        self->send(proto::encodeGachaResult(spent, message, spent ? rolled.dex : std::uint16_t{0},
                                            rolled.rarity, duplicate));

        if (!spent) {
            // 토큰이 안 나갔으면 보유량도 그대로다. 그래도 화면과 어긋나 있을 수
            // 있으니(다른 접속이 썼을 수 있다) 진짜 값을 한 번 보낸다.
            std::uint32_t tokens = 0;
            if (context->characters->tokenBalance(accountId, characterId, tokens)) {
                self->send(proto::encodeTokenBalance(tokens));
            }
            return;
        }

        self->send(proto::encodeTokenBalance(tokensLeft));

        if (result == data::GachaResult::Granted) {
            // 로스터가 늘었다. 파티 화면의 후보 목록을 새로 보낸다.
            fieldshared::sendPartyState(context->characters, *self, accountId, characterId, true,
                                        "");
            spdlog::info("gacha: character {} unlocked dex {} ({} left)", characterId,
                         rolled.dex, tokensLeft);
        } else {
            spdlog::info("gacha: character {} drew a duplicate dex {} ({} left)", characterId,
                         rolled.dex, tokensLeft);
        }
    });

    if (!queued) {
        spdlog::warn("{}: db queue full, refusing gacha draw", session.peer());
        session.send(proto::encodeGachaResult(false, "서버가 혼잡합니다", 0,
                                              proto::GachaRarity::Normal, false));
    }
    return true;
}

// 토큰 수급처가 아직 없어서 둔 디버그 경로다.
//
// ponytail: 아무나 자기 토큰을 늘릴 수 있다. 지금은 혼자 돌리는 개발 서버라
// 그대로 두지만, 수급처(퀘스트·상점 등)가 생기면 이 핸들러와 field.fbs 의
// DebugGrantToken 을 **같이** 지운다. 남겨 두면 그게 곧 치트 경로다.
bool FieldHandler::handleDebugGrantToken(TlsSession &session) {
    if (context_.characters == nullptr) {
        return true;
    }

    auto self = session.shared_from_this();
    const FieldContext *context = &context_;
    const std::uint64_t accountId = accountId_;
    const std::uint64_t characterId = characterId_;

    const bool queued = context_.dbQueue->submit([self, context, accountId, characterId] {
        std::uint32_t tokensLeft = 0;
        if (!context->characters->grantToken(accountId, characterId, tokensLeft)) {
            spdlog::warn("debug token grant failed for character {}", characterId);
            return;
        }
        self->send(proto::encodeTokenBalance(tokensLeft));
        spdlog::warn("debug token granted to character {} ({} total)", characterId, tokensLeft);
    });

    if (!queued) {
        spdlog::warn("{}: db queue full, refusing debug token", session.peer());
    }
    return true;
}

bool FieldHandler::handleMove(TlsSession &session, const HeavenField::Move &request) {
    return context_.world->move(characterId_, &session, hhv::movement::wire::decodeInputs(request));
}

void FieldHandler::onClosed(TlsSession &session) {
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

    spdlog::info("left: {} ({}) - {} in field", nickname_, session.peer(), context_.world->size());

    // 개발 모드에는 저장소가 없다. 위치는 프로세스와 함께 사라진다.
    if (context_.devNoAuth) {
        return;
    }

    // DB 쓰기라 IOCP 워커에서 하지 않는다.
    const FieldContext *context = &context_;
    const std::uint64_t characterId = characterId_;
    const data::Position position = *last;

    context_.dbQueue->submit([context, characterId, position] {
        context->characters->savePosition(characterId, position);
        if (context->redis != nullptr) {
            clearRedisPosition(*context->redis, characterId);
        }
    });
}

} // namespace heaven::field
