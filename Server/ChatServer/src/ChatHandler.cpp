#include "ChatHandler.h"

#include <spdlog/spdlog.h>

// 닉네임 길이 규칙의 주인은 로그인 서버다. 여기서 값을 다시 적으면 한쪽만 바뀐다.
#include "LoginCodec.h"

#include <chrono>

namespace heaven::chat {

namespace {

// 파티 명령이 실패했을 때 본인에게만 보내는 안내.
void notice(TlsSession& session, std::string_view text) {
    session.send(proto::encodeNotice(text));
}

}  // namespace

bool ChatHandler::onFrame(TlsSession& session, const proto::Bytes& body) {
    return joined_ ? handleAuthenticated(session, body) : handleHello(session, body);
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
        proto::verifyTicket(ticket, proto::kAudienceChat, keys_, proto::nowUnix(), verified);

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
    const auto displaced = room_.join(accountId_, nickname_, self);

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

    // 레벨을 옮기면 이 연결은 끊겼다가 다시 붙는다. 파티가 살아 있으면 그대로
    // 실어 보내야 화면이 복구된다.
    if (party_ != nullptr) {
        party_->touch(accountId_);
        sendPartyState(session, "");
    }
    return true;
}

bool ChatHandler::handleAuthenticated(TlsSession& session, const proto::Bytes& body) {
    const auto* envelope = proto::verifyEnvelope(body);
    if (envelope == nullptr) {
        spdlog::warn("{}: dropped malformed frame", nickname_);
        return true;  // 한 프레임 버리고 계속
    }

    switch (envelope->payload_type()) {
        case HeavenChat::Payload::Say:
            return handleSay(*envelope->payload_as_Say());
        case HeavenChat::Payload::SayParty:
            return handleSayParty(session, *envelope->payload_as_SayParty());
        case HeavenChat::Payload::SayInstance:
            return handleSayInstance(session, *envelope->payload_as_SayInstance());
        case HeavenChat::Payload::PartyInvite:
            handleInvite(session, *envelope->payload_as_PartyInvite());
            return true;
        case HeavenChat::Payload::PartyAccept:
            handleAccept(session, *envelope->payload_as_PartyAccept());
            return true;
        case HeavenChat::Payload::PartyDecline:
            handleDecline(*envelope->payload_as_PartyDecline());
            return true;
        case HeavenChat::Payload::PartyLeave:
            handleLeave(session);
            return true;
        case HeavenChat::Payload::PartyKick:
            handleKick(session, *envelope->payload_as_PartyKick());
            return true;
        case HeavenChat::Payload::PartyEnterInstance:
            handleEnterInstance(session, *envelope->payload_as_PartyEnterInstance());
            return true;
        default:
            spdlog::warn("{}: unexpected payload type from client", nickname_);
            return true;
    }
}

bool ChatHandler::throttled() {
    // 도배 차단. 버리기만 하고 끊지는 않는다 — 랙으로 몰려 온 정상 입력도 있다.
    const auto now = std::chrono::steady_clock::now();
    if (now - lastSayAt_ < proto::kMinSayInterval) {
        return true;
    }
    lastSayAt_ = now;
    return false;
}

bool ChatHandler::handleSay(const HeavenChat::Say& request) {
    const auto* text = request.text();
    if (text == nullptr || text->size() == 0) {
        return true;
    }

    // 한 사람이 보낸 바이트가 접속자 수만큼 증폭돼 나간다. 프레임 상한(64KiB)에만
    // 기대면 한 장으로 방 전체의 전송 큐를 채울 수 있다.
    if (text->size() > proto::kMaxChatTextBytes) {
        spdlog::warn("{}: dropped a {} byte message", nickname_, text->size());
        return true;
    }
    if (throttled()) {
        return true;
    }

    spdlog::info("[{}] {}", nickname_, text->str());
    room_.broadcast(proto::encodeChat(nickname_, text->str()), nullptr);
    return true;
}

bool ChatHandler::handleSayParty(TlsSession& session, const HeavenChat::SayParty& request) {
    const auto* text = request.text();
    if (text == nullptr || text->size() == 0) {
        return true;
    }
    if (text->size() > proto::kMaxChatTextBytes) {
        spdlog::warn("{}: dropped a {} byte party message", nickname_, text->size());
        return true;
    }
    if (party_ == nullptr) {
        notice(session, "파티 기능을 쓸 수 없습니다");
        return true;
    }
    if (throttled()) {
        return true;
    }

    const party::PartyView view = party_->find(accountId_);
    if (!view.valid()) {
        notice(session, "파티에 속해 있지 않습니다");
        return true;
    }

    spdlog::info("[파티 {}] [{}] {}", view.id, nickname_, text->str());
    room_.sendToAccounts(view.accountIds(), proto::encodePartyChat(nickname_, text->str()));
    return true;
}

bool ChatHandler::handleSayInstance(TlsSession& session,
                                   const HeavenChat::SayInstance& request) {
    const auto* text = request.text();
    if (text == nullptr || text->size() == 0) {
        return true;
    }
    if (text->size() > proto::kMaxChatTextBytes) {
        spdlog::warn("{}: dropped a {} byte instance message", nickname_, text->size());
        return true;
    }
    if (presence_ == nullptr) {
        notice(session, "인스턴스 채팅을 쓸 수 없습니다");
        return true;
    }
    if (throttled()) {
        return true;
    }

    // 방 안에 있는 사람만 받는다. 어느 방인지는 InstanceServer 가 적어 둔 것을 읽는다.
    const std::vector<std::uint64_t> listeners = presence_->roommates(accountId_);
    if (listeners.empty()) {
        notice(session, "인스턴스 안에서만 쓸 수 있습니다");
        return true;
    }

    spdlog::info("[인스턴스] [{}] {}", nickname_, text->str());
    room_.sendToAccounts(listeners, proto::encodeInstanceChat(nickname_, text->str()));
    return true;
}

void ChatHandler::sendPartyState(TlsSession& session, std::string_view message) {
    if (party_ == nullptr) {
        return;
    }
    const party::PartyView view = party_->find(accountId_);

    std::vector<proto::PartyMemberInfo> members;
    members.reserve(view.members.size());
    for (const party::PartyMember& member : view.members) {
        members.push_back({member.accountId, member.nickname});
    }
    session.send(proto::encodePartyState(view.id, members, message));
}

void ChatHandler::broadcastPartyState(const party::PartyView& view, std::string_view message) {
    if (!view.valid()) {
        return;
    }
    std::vector<proto::PartyMemberInfo> members;
    members.reserve(view.members.size());
    for (const party::PartyMember& member : view.members) {
        members.push_back({member.accountId, member.nickname});
    }
    room_.sendToAccounts(view.accountIds(), proto::encodePartyState(view.id, members, message));
}

void ChatHandler::handleInvite(TlsSession& session, const HeavenChat::PartyInvite& request) {
    if (party_ == nullptr) {
        notice(session, "파티 기능을 쓸 수 없습니다");
        return;
    }

    const auto* name = request.target_nickname();
    if (name == nullptr || name->size() == 0 || name->size() > proto::kMaxNicknameBytes) {
        notice(session, "초대할 닉네임이 올바르지 않습니다");
        return;
    }

    // 닉네임으로만 찾는다. 계정 번호는 클라이언트가 알 방법이 없고, 알게 해서도
    // 안 된다. 접속 중이 아니면 초대장을 남길 대상도 정할 수 없다.
    const std::uint64_t target = room_.accountByNickname(name->str());
    if (target == 0) {
        notice(session, name->str() + " 님은 접속 중이 아닙니다");
        return;
    }
    if (target == accountId_) {
        notice(session, "자기 자신은 초대할 수 없습니다");
        return;
    }

    std::uint64_t partyId = 0;
    switch (party_->invite(accountId_, nickname_, target, partyId)) {
        case party::InviteResult::Full:
            notice(session, "파티가 가득 찼습니다");
            return;
        case party::InviteResult::TargetBusy:
            notice(session, name->str() + " 님은 이미 다른 파티에 있습니다");
            return;
        case party::InviteResult::Failed:
            notice(session, "초대에 실패했습니다");
            return;
        case party::InviteResult::Ok:
            break;
    }

    if (const auto peer = room_.findAccount(target)) {
        peer->send(proto::encodePartyInvited(partyId, nickname_));
    }
    notice(session, name->str() + " 님을 초대했습니다");

    // 초대를 하면서 파티가 막 생겼을 수 있다. 본인 화면도 갱신한다.
    sendPartyState(session, "");
}

void ChatHandler::handleAccept(TlsSession& session, const HeavenChat::PartyAccept& request) {
    if (party_ == nullptr) {
        notice(session, "파티 기능을 쓸 수 없습니다");
        return;
    }

    party::PartyView view;
    switch (party_->accept(accountId_, nickname_, request.party_id(), view)) {
        case party::AcceptResult::NoInvite:
            notice(session, "초대가 만료되었습니다");
            return;
        case party::AcceptResult::AlreadyInParty:
            notice(session, "이미 파티에 속해 있습니다");
            return;
        case party::AcceptResult::Gone:
            notice(session, "파티가 사라졌습니다");
            return;
        case party::AcceptResult::Full:
            notice(session, "파티가 가득 찼습니다");
            return;
        case party::AcceptResult::Failed:
            notice(session, "파티 참가에 실패했습니다");
            return;
        case party::AcceptResult::Ok:
            break;
    }

    spdlog::info("party {}: {} joined ({} members)", view.id, nickname_, view.members.size());
    broadcastPartyState(view, nickname_ + " 님이 파티에 들어왔습니다");
}

void ChatHandler::handleDecline(const HeavenChat::PartyDecline& request) {
    if (party_ != nullptr) {
        party_->decline(accountId_, request.party_id());
    }
}

void ChatHandler::handleLeave(TlsSession& session) {
    if (party_ == nullptr) {
        return;
    }
    const std::uint64_t partyId = party_->leave(accountId_);
    if (partyId == 0) {
        return;
    }

    spdlog::info("party {}: {} left", partyId, nickname_);

    // 본인에게는 "파티 없음"을, 남은 사람에게는 줄어든 명단을 보낸다.
    // 파티장이 빠졌으면 남은 목록의 첫 번째가 그대로 새 파티장이다.
    sendPartyState(session, "파티에서 나왔습니다");
    broadcastPartyState(party_->findById(partyId), nickname_ + " 님이 파티에서 나갔습니다");
}

void ChatHandler::handleKick(TlsSession& session, const HeavenChat::PartyKick& request) {
    if (party_ == nullptr) {
        notice(session, "파티 기능을 쓸 수 없습니다");
        return;
    }

    const party::PartyView before = party_->find(accountId_);
    if (!before.valid()) {
        notice(session, "파티에 속해 있지 않습니다");
        return;
    }
    if (!before.isLeader(accountId_)) {
        notice(session, "파티장만 내보낼 수 있습니다");
        return;
    }

    const std::uint64_t target = request.target_account();
    if (target == accountId_) {
        notice(session, "자기 자신은 내보낼 수 없습니다");
        return;
    }
    if (!before.contains(target)) {
        notice(session, "파티원이 아닙니다");
        return;
    }

    party_->leave(target);
    if (const auto peer = room_.findAccount(target)) {
        peer->send(proto::encodePartyState(0, {}, "파티에서 내보내졌습니다"));
    }
    broadcastPartyState(party_->findById(before.id), "파티원을 내보냈습니다");
}

void ChatHandler::handleEnterInstance(TlsSession& session,
                                      const HeavenChat::PartyEnterInstance& request) {
    if (party_ == nullptr) {
        notice(session, "파티 기능을 쓸 수 없습니다");
        return;
    }

    const party::PartyView view = party_->find(accountId_);
    if (!view.valid()) {
        // 파티가 없으면 혼자 들어가면 된다. 채팅 서버가 할 일이 없다.
        notice(session, "파티에 속해 있지 않습니다");
        return;
    }
    if (!view.isLeader(accountId_)) {
        notice(session, "파티장만 입장을 시작할 수 있습니다");
        return;
    }

    const std::uint32_t type = request.instance_type();
    if (!party_->openEntry(view.id, type)) {
        notice(session, "입장을 시작하지 못했습니다");
        return;
    }

    spdlog::info("party {}: leader {} opened instance type {}", view.id, nickname_, type);
    room_.sendToAccounts(view.accountIds(), proto::encodePartyInstanceReady(type));
}

void ChatHandler::onClosed(TlsSession& session) {
    if (!joined_) {
        spdlog::debug("closed before hello: {}", session.peer());
        return;
    }
    joined_ = false;

    auto self = session.shared_from_this();
    room_.leave(accountId_, self);

    // 파티에서 바로 빼지 않는다. 레벨을 옮길 때도 이 연결은 끊기므로, 여기서
    // 빼면 다 같이 인스턴스로 들어가려는 그 동작이 파티를 해산시킨다.
    // 생존 신호(party:member:)의 TTL 이 알아서 판정한다 — PartyStore 주석 참고.

    spdlog::info("left: {} ({}) - {} online", nickname_, session.peer(), room_.size());
    room_.broadcast(proto::encodeNotice(nickname_ + " 님이 나갔습니다"), nullptr);
}

}  // namespace heaven::chat
