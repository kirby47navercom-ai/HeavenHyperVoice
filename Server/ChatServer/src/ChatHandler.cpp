#include "ChatHandler.h"

#include <spdlog/spdlog.h>

#include <atomic>
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

// 이 프로세스 안에서 세션을 구분하는 값. 레지스트리에서 "아직 내 것인가" 를
// 판단할 때 쓰므로 재사용되지 않기만 하면 된다.
std::string nextSessionId() {
    static std::atomic<std::uint64_t> counter{0};
    return std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
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
    sessionId_ = nextSessionId();
    joined_ = true;
    session.markAuthenticated();

    // 레지스트리에 자기 것으로 등록하고 이전 주인을 받아온다 (후접속 우선).
    // 서버가 한 종류뿐이라 실제 강제 종료는 아래 Room 인덱스가 처리한다.
    // 레지스트리는 클라이언트가 직접 붙는 프로세스가 늘었을 때를 위한 것이다.
    if (registry_ != nullptr) {
        const auto previous = registry_->claim(accountId_, sessionId_);
        if (previous.has_value() && previous->serverId != registry_->serverId()) {
            spdlog::info("{} was connected to {}; that server owns the disconnect", nickname_,
                         previous->serverId);
        }
    }

    auto self = session.shared_from_this();
    const auto displaced = room_.join(accountId_, self);

    if (displaced) {
        spdlog::info("{} (account {}) logged in again; dropping the earlier session at {}",
                     nickname_, accountId_, displaced->peer());
        displaced->send(proto::encodeNotice("다른 곳에서 로그인하여 연결을 종료합니다"));
        displaced->closeAfterFlush();
    }

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

    auto self = session.shared_from_this();
    room_.leave(accountId_, self);

    // 아직 내 것일 때만 지운다. 자리를 넘겨준 뒤라면 새 주인의 등록이 남아야 한다.
    if (registry_ != nullptr) {
        registry_->release(accountId_, sessionId_);
    }

    spdlog::info("left: {} ({}) - {} online", nickname_, session.peer(), room_.size());
    room_.broadcast(makeFrame(proto::encodeNotice(nickname_ + " 님이 나갔습니다")), nullptr);
}

}  // namespace heaven::chat
