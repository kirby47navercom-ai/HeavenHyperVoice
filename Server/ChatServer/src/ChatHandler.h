#pragma once

#include <cstdint>
#include <string>

#include "AuthTicket.h"
#include "ChatCodec.h"
#include "FrameHandler.h"
#include "Room.h"
#include "SessionRegistry.h"

namespace heaven::chat {

using net::FrameHandler;

// 세션 하나의 채팅 상태.
// onFrame/onClosed 는 세션 락 밖에서, 한 세션당 한 스레드에서 순서대로 불린다.
// 따라서 이 클래스의 멤버에는 별도 동기화가 필요 없다.
class ChatHandler : public FrameHandler {
public:
    ChatHandler(Room& room, const proto::PublicKeyRing& keys, SessionRegistry* registry)
        : room_(room), keys_(keys), registry_(registry) {}

    bool onFrame(TlsSession& session, const proto::Bytes& body) override;
    void onClosed(TlsSession& session) override;

private:
    bool handleHello(TlsSession& session, const proto::Bytes& body);
    bool handleSay(const proto::Bytes& body);

    Room& room_;
    const proto::PublicKeyRing& keys_;
    SessionRegistry* registry_;  // 없을 수 있다 (--no-redis 또는 접속 실패)

    std::string nickname_;
    std::string sessionId_;
    std::uint64_t accountId_ = 0;
    std::uint64_t characterId_ = 0;
    bool joined_ = false;
};

}  // namespace heaven::chat
