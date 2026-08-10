#include "ChatHandler.h"

#include <spdlog/spdlog.h>

#include <chrono>

namespace heaven::chat {

namespace {

std::int64_t nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

bool ChatHandler::onFrame(TlsSession& session, const proto::Bytes& body) {
    return joined_ ? handleSay(body) : handleHello(session, body);
}

bool ChatHandler::handleHello(TlsSession& session, const proto::Bytes& body) {
    const auto* envelope = proto::verifyEnvelope(body);
    if (envelope == nullptr || envelope->payload_type() != HeavenChat::Payload::Hello) {
        spdlog::warn("{}: first frame was not a valid Hello", session.peer());
        session.send(proto::encodeNotice("잘못된 Hello 프레임입니다"));
        return false;
    }

    const auto* blob = envelope->payload_as_Hello()->ticket();
    if (blob == nullptr || blob->size() == 0) {
        spdlog::warn("{}: Hello carried no ticket", session.peer());
        session.send(proto::encodeNotice("로그인이 필요합니다"));
        return false;
    }

    const proto::Bytes ticket(blob->begin(), blob->end());
    proto::VerifiedTicket verified;
    const proto::TicketError error =
        proto::verifyTicket(ticket, proto::kAudienceChat, keys_, nowUnix(), verified);

    if (error != proto::TicketError::Ok) {
        spdlog::warn("{}: ticket rejected - {}", session.peer(), proto::describe(error));
        session.send(proto::encodeNotice(std::string("인증 실패: ") + proto::describe(error)));
        return false;
    }

    // 닉네임은 서명된 티켓에서 온다. 클라이언트가 주장한 값이 아니다.
    nickname_ = verified.nickname;
    accountId_ = verified.accountId;
    characterId_ = verified.characterId;
    joined_ = true;
    session.markAuthenticated();

    auto self = session.shared_from_this();
    const auto displaced = room_.join(accountId_, self);

    if (displaced) {
        spdlog::info("{} (account {}) logged in again; dropping the earlier session at {}",
                     nickname_, accountId_, displaced->peer());
        displaced->send(proto::encodeNotice("다른 곳에서 로그인하여 연결을 종료합니다"));
        displaced->closeAfterFlush();
    }

    // 중복 접속 판정은 계정 단위다. 한 계정으로 캐릭터 여럿을 동시에
    // 붙이는 것도 막아야 하므로 characterId 로 좁히면 안 된다.
    spdlog::info("joined: {} (character {}, account {} via {}, {}) - {} online", nickname_,
                 characterId_, accountId_, verified.issuer, session.peer(), room_.size());
    room_.broadcast(proto::encodeNotice(nickname_ + " 님이 들어왔습니다"), &session);
    return true;
}

bool ChatHandler::handleSay(const proto::Bytes& body) {
    const auto* envelope = proto::verifyEnvelope(body);
    if (envelope == nullptr) {
        spdlog::warn("{}: dropped malformed frame", nickname_);
        return true;  // 한 프레임 버리고 계속
    }
    if (envelope->payload_type() != HeavenChat::Payload::Say) {
        spdlog::warn("{}: unexpected payload type from client", nickname_);
        return true;
    }

    const auto* text = envelope->payload_as_Say()->text();
    if (text == nullptr || text->size() == 0) {
        return true;
    }

    // 한 사람이 보낸 바이트가 접속자 수만큼 증폭돼 나간다. 프레임 상한(64KiB)에만
    // 기대면 한 장으로 방 전체의 전송 큐를 채울 수 있다.
    if (text->size() > proto::kMaxChatTextBytes) {
        spdlog::warn("{}: dropped a {} byte message", nickname_, text->size());
        return true;
    }

    // 도배 차단. 버리기만 하고 끊지는 않는다 — 랙으로 몰려 온 정상 입력도 있다.
    const auto now = std::chrono::steady_clock::now();
    if (now - lastSayAt_ < proto::kMinSayInterval) {
        return true;
    }
    lastSayAt_ = now;

    spdlog::info("[{}] {}", nickname_, text->str());
    room_.broadcast(proto::encodeChat(nickname_, text->str()), nullptr);
    return true;
}

void ChatHandler::onClosed(TlsSession& session) {
    if (!joined_) {
        spdlog::debug("closed before hello: {}", session.peer());
        return;
    }
    joined_ = false;

    auto self = session.shared_from_this();
    room_.leave(accountId_, self);

    spdlog::info("left: {} ({}) - {} online", nickname_, session.peer(), room_.size());
    room_.broadcast(proto::encodeNotice(nickname_ + " 님이 나갔습니다"), nullptr);
}

}  // namespace heaven::chat
