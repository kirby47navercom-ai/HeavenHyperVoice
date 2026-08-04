#pragma once

#include <cstdint>
#include <string>

#include "AccountStore.h"
#include "AuthTicket.h"
#include "FrameHandler.h"
#include "LoginCodec.h"
#include "TlsSession.h"
#include "WorkQueue.h"

namespace heaven::login {

using net::FrameHandler;
using net::TlsSession;

// 로그인 서버가 클라이언트에게 알려주는 채팅 서버 위치.
struct ChatEndpoint {
    std::string host;
    std::uint16_t port = 0;
};

// 티켓 발급에 필요한, 세션마다 바뀌지 않는 것들.
struct LoginContext {
    AccountStore* accounts = nullptr;
    const proto::TicketSigner* signer = nullptr;
    WorkQueue* authQueue = nullptr;
    ChatEndpoint chat;
    std::int64_t ticketTtlSeconds = 60;
    std::string issuer;
    std::string audience;
};

// 세션 하나의 로그인 처리. 요청 하나를 받고 응답한 뒤 연결을 끊는다.
//
// onFrame 은 파싱만 하고 즉시 반환한다. 계정 조회와 비밀번호 검증은
// 인증 스레드에서 돌아가며, 응답도 거기서 보낸다.
class LoginHandler : public FrameHandler {
public:
    explicit LoginHandler(const LoginContext& context) : context_(context) {}

    bool onFrame(TlsSession& session, const proto::Bytes& body) override;

private:
    bool handleLogin(TlsSession& session, const HeavenLogin::LoginRequest& request);
    bool handleRegister(TlsSession& session, const HeavenLogin::RegisterRequest& request);

    const LoginContext& context_;
    bool handled_ = false;
};

}  // namespace heaven::login
