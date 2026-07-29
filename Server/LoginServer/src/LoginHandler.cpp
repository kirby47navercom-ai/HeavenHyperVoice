#include "LoginHandler.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <utility>

#include "LoginCodec.h"

namespace heaven::login {

namespace {

std::int64_t nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

LoginHandler::LoginHandler(AccountStore& accounts, const proto::TicketSigner& signer,
                           const ChatEndpoint& chat, std::int64_t ticketTtlSeconds,
                           std::string issuer, std::string audience)
    : accounts_(accounts),
      signer_(signer),
      chat_(chat),
      ticketTtlSeconds_(ticketTtlSeconds),
      issuer_(std::move(issuer)),
      audience_(std::move(audience)) {}

bool LoginHandler::onFrame(TlsSession& session, const proto::Bytes& body) {
    if (handled_) {
        spdlog::warn("{}: extra frame after login, dropping connection", session.peer());
        return false;
    }
    handled_ = true;

    const auto* envelope = proto::verifyLoginEnvelope(body);
    if (envelope == nullptr || envelope->payload_type() != HeavenLogin::Payload::LoginRequest) {
        spdlog::warn("{}: first frame was not a valid LoginRequest", session.peer());
        session.send(proto::encodeLoginFailure("잘못된 요청입니다"));
        session.closeAfterFlush();
        return true;
    }

    const auto* request = envelope->payload_as_LoginRequest();
    const auto* username = request->username();
    const auto* password = request->password();

    if (username == nullptr || username->size() == 0 ||
        username->size() > proto::kMaxUsernameBytes ||
        (password != nullptr && password->size() > proto::kMaxPasswordBytes)) {
        spdlog::warn("{}: rejected malformed credentials", session.peer());
        session.send(proto::encodeLoginFailure("아이디 또는 비밀번호 형식이 올바르지 않습니다"));
        session.closeAfterFlush();
        return true;
    }

    // 첫 유효 프레임을 받았으므로 핸드셰이크 타임아웃 대상에서 제외한다.
    session.markAuthenticated();

    const std::string user = username->str();
    const auto account = accounts_.authenticate(
        user, password != nullptr ? password->string_view() : std::string_view{});

    if (!account.has_value()) {
        spdlog::info("login rejected: {} ({})", user, session.peer());
        session.send(proto::encodeLoginFailure("아이디 또는 비밀번호가 올바르지 않습니다"));
        session.closeAfterFlush();
        return true;
    }

    proto::TicketClaims claims;
    claims.issuer = issuer_;
    claims.audience = audience_;
    claims.accountId = account->id;
    claims.nickname = account->nickname;
    claims.issuedUnix = nowUnix();
    claims.expiresUnix = claims.issuedUnix + ticketTtlSeconds_;

    proto::Bytes ticket;
    try {
        ticket = signer_.sign(claims);
    } catch (const std::exception& e) {
        spdlog::error("failed to sign ticket for {}: {}", user, e.what());
        session.send(proto::encodeLoginFailure("서버 오류로 로그인에 실패했습니다"));
        session.closeAfterFlush();
        return true;
    }

    spdlog::info("login ok: {} (account {}, {}) - ticket valid {}s", account->nickname,
                 account->id, session.peer(), ticketTtlSeconds_);

    session.send(
        proto::encodeLoginSuccess(ticket, chat_.host, chat_.port, account->nickname));
    session.closeAfterFlush();
    return true;
}

}  // namespace heaven::login
