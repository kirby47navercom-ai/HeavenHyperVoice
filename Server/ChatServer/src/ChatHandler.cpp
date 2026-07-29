#include "ChatHandler.h"

#include <spdlog/spdlog.h>

#include <chrono>

namespace heaven::chat {

namespace {

Frame makeFrame(proto::Bytes bytes) {
    return std::make_shared<const proto::Bytes>(std::move(bytes));
}

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
    joined_ = true;
    session.markAuthenticated();

    room_.join(session.shared_from_this());
    spdlog::info("joined: {} (account {} via {}, {}) - {} online", nickname_, accountId_,
                 verified.issuer, session.peer(), room_.size());
    room_.broadcast(makeFrame(proto::encodeNotice(nickname_ + " 님이 들어왔습니다")), &session);
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

    spdlog::info("[{}] {}", nickname_, text->str());
    room_.broadcast(makeFrame(proto::encodeChat(nickname_, text->str())), nullptr);
    return true;
}

void ChatHandler::onClosed(TlsSession& session) {
    if (!joined_) {
        spdlog::debug("closed before hello: {}", session.peer());
        return;
    }
    joined_ = false;

    room_.leave(session.shared_from_this());
    spdlog::info("left: {} ({}) - {} online", nickname_, session.peer(), room_.size());
    room_.broadcast(makeFrame(proto::encodeNotice(nickname_ + " 님이 나갔습니다")), nullptr);
}

}  // namespace heaven::chat
