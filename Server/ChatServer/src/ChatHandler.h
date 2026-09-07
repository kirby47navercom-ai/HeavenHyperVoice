#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "AuthTicket.h"
#include "ChatCodec.h"
#include "FrameHandler.h"
#include "InstancePresence.h"
#include "PartyStore.h"
#include "Room.h"

namespace heaven::chat {

using net::FrameHandler;

// 세션 하나의 채팅 상태.
// onFrame/onClosed 는 세션 락 밖에서, 한 세션당 한 스레드에서 순서대로 불린다.
// 따라서 이 클래스의 멤버에는 별도 동기화가 필요 없다.
//
// 파티를 **바꾸는** 명령은 전부 여기 있다. 필드에서든 인스턴스에서든 채팅 연결은
// 살아 있어서 어디서나 파티를 만질 수 있고, 쓰기가 한 서버에만 있으면 경쟁
// 조건도 한 곳에만 생긴다. Field/Instance 는 Redis 를 읽기만 한다.
class ChatHandler : public FrameHandler {
public:
    // party 는 Redis 가 없으면 nullptr 이다. 그때는 파티 명령이 전부 거절된다 —
    // 채팅 자체는 계속 된다.
    ChatHandler(Room& room, const proto::PublicKeyRing& keys, party::PartyStore* party,
                instancechat::InstancePresence* presence)
        : room_(room), keys_(keys), party_(party), presence_(presence) {}

    bool onFrame(TlsSession& session, const proto::Bytes& body) override;
    void onClosed(TlsSession& session) override;

private:
    bool handleHello(TlsSession& session, const proto::Bytes& body);
    bool handleAuthenticated(TlsSession& session, const proto::Bytes& body);
    bool handleSay(const HeavenChat::Say& request);
    bool handleSayParty(TlsSession& session, const HeavenChat::SayParty& request);
    bool handleSayInstance(TlsSession& session, const HeavenChat::SayInstance& request);

    void handleInvite(TlsSession& session, const HeavenChat::PartyInvite& request);
    void handleAccept(TlsSession& session, const HeavenChat::PartyAccept& request);
    void handleDecline(const HeavenChat::PartyDecline& request);
    void handleLeave(TlsSession& session);
    void handleKick(TlsSession& session, const HeavenChat::PartyKick& request);
    void handleEnterInstance(TlsSession& session,
                             const HeavenChat::PartyEnterInstance& request);

    // 지금 파티를 읽어 본인에게 보낸다. 파티가 없으면 party_id 0 을 보낸다 —
    // 클라이언트가 "없음"을 명시적으로 알아야 화면을 지울 수 있다.
    void sendPartyState(TlsSession& session, std::string_view message);

    // 파티 전원에게 갱신된 상태를 알린다. 접속 중이 아닌 멤버는 건너뛴다.
    void broadcastPartyState(const party::PartyView& view, std::string_view message);

    // 도배 차단에 걸리면 true. 걸린 프레임은 버리고 연결은 유지한다.
    bool throttled();

    Room& room_;
    const proto::PublicKeyRing& keys_;
    party::PartyStore* party_ = nullptr;

    // 누가 어느 인스턴스 방에 있는지. 여기서는 읽기만 한다 — 쓰는 것은
    // InstanceServer 다. Redis 가 없으면 nullptr 이고 인스턴스 채팅이 거절된다.
    instancechat::InstancePresence* presence_ = nullptr;

    std::string nickname_;
    std::uint64_t accountId_ = 0;
    std::uint64_t characterId_ = 0;
    bool joined_ = false;

    // 도배 차단. 한 세션의 프레임은 한 스레드에서 순서대로 오므로 락이 필요 없다.
    std::chrono::steady_clock::time_point lastSayAt_{};
};

}  // namespace heaven::chat
