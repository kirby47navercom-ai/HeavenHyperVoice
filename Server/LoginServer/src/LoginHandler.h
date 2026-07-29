#pragma once

#include <cstdint>
#include <string>

#include "AccountStore.h"
#include "AuthTicket.h"
#include "FrameHandler.h"
#include "TlsSession.h"

namespace heaven::login {

using net::FrameHandler;
using net::TlsSession;

// 로그인 서버가 클라이언트에게 알려주는 채팅 서버 위치.
struct ChatEndpoint {
    std::string host;
    std::uint16_t port = 0;
};

// 세션 하나의 로그인 처리. 요청 하나를 받고 응답한 뒤 연결을 끊는다.
class LoginHandler : public FrameHandler {
public:
    LoginHandler(AccountStore& accounts, const proto::TicketSigner& signer,
                 const ChatEndpoint& chat, std::int64_t ticketTtlSeconds, std::string issuer,
                 std::string audience);

    bool onFrame(TlsSession& session, const proto::Bytes& body) override;

private:
    AccountStore& accounts_;
    const proto::TicketSigner& signer_;
    const ChatEndpoint& chat_;
    std::int64_t ticketTtlSeconds_;
    std::string issuer_;
    std::string audience_;
    bool handled_ = false;
};

}  // namespace heaven::login
