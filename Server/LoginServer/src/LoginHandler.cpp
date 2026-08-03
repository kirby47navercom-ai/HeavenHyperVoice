#include "LoginHandler.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <memory>
#include <utility>

#include "LoginCodec.h"

namespace heaven::login {

namespace {

std::int64_t nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void fail(TlsSession& session, std::string_view message) {
    session.send(proto::encodeLoginFailure(message));
    session.closeAfterFlush();
}

}  // namespace

bool LoginHandler::onFrame(TlsSession& session, const proto::Bytes& body) {
    if (handled_) {
        spdlog::warn("{}: extra frame after login, dropping connection", session.peer());
        return false;
    }
    handled_ = true;

    const auto* envelope = proto::verifyLoginEnvelope(body);
    if (envelope == nullptr || envelope->payload_type() != HeavenLogin::Payload::LoginRequest) {
        spdlog::warn("{}: first frame was not a valid LoginRequest", session.peer());
        fail(session, "잘못된 요청입니다");
        return true;
    }

    const auto* request = envelope->payload_as_LoginRequest();
    const auto* username = request->username();
    const auto* password = request->password();

    if (username == nullptr || username->size() == 0 ||
        username->size() > proto::kMaxUsernameBytes ||
        (password != nullptr && password->size() > proto::kMaxPasswordBytes)) {
        spdlog::warn("{}: rejected malformed credentials", session.peer());
        fail(session, "아이디 또는 비밀번호 형식이 올바르지 않습니다");
        return true;
    }

    // 첫 유효 프레임을 받았으므로 핸드셰이크 타임아웃 대상에서 제외한다.
    session.markAuthenticated();

    // 여기서부터는 인증 스레드로 넘긴다. IOCP 워커는 즉시 반환한다.
    // 람다가 세션을 shared_ptr 로 붙잡으므로, 작업이 끝날 때까지 세션과
    // 이 핸들러(세션이 소유한다)가 살아 있다.
    auto self = session.shared_from_this();
    std::string user = username->str();
    std::string secret = password != nullptr ? password->str() : std::string();

    // context 는 main 이 소유하고 서버보다 오래 산다. 포인터를 값으로 복사한다.
    // 지역 참조를 참조로 캡처하면 onFrame 이 반환되는 순간 매달린 참조가 된다.
    const LoginContext* context = &context_;

    context_.authQueue->submit([self, context, user = std::move(user),
                                secret = std::move(secret)] {
        const auto account = context->accounts->authenticate(user, secret);

        if (!account.has_value()) {
            spdlog::info("login rejected: {} ({})", user, self->peer());
            self->send(proto::encodeLoginFailure("아이디 또는 비밀번호가 올바르지 않습니다"));
            self->closeAfterFlush();
            return;
        }

        proto::TicketClaims claims;
        claims.issuer = context->issuer;
        claims.audience = context->audience;
        claims.accountId = account->id;
        claims.nickname = account->nickname;
        claims.issuedUnix = nowUnix();
        claims.expiresUnix = claims.issuedUnix + context->ticketTtlSeconds;

        proto::Bytes ticket;
        try {
            ticket = context->signer->sign(claims);
        } catch (const std::exception& e) {
            spdlog::error("failed to sign ticket for {}: {}", user, e.what());
            self->send(proto::encodeLoginFailure("서버 오류로 로그인에 실패했습니다"));
            self->closeAfterFlush();
            return;
        }

        spdlog::info("login ok: {} (account {}, {}) - ticket valid {}s", account->nickname,
                     account->id, self->peer(), context->ticketTtlSeconds);

        self->send(proto::encodeLoginSuccess(ticket, context->chat.host, context->chat.port,
                                             account->nickname));
        self->closeAfterFlush();
    });

    return true;
}

}  // namespace heaven::login
