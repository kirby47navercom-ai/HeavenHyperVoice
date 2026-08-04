#include "LoginHandler.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <memory>
#include <utility>

#include "LoginCodec.h"
#include "PasswordHash.h"

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
    if (envelope == nullptr) {
        spdlog::warn("{}: malformed first frame", session.peer());
        fail(session, "잘못된 요청입니다");
        return true;
    }

    switch (envelope->payload_type()) {
        case HeavenLogin::Payload::LoginRequest:
            return handleLogin(session, *envelope->payload_as_LoginRequest());
        case HeavenLogin::Payload::RegisterRequest:
            return handleRegister(session, *envelope->payload_as_RegisterRequest());
        default:
            spdlog::warn("{}: unexpected payload type", session.peer());
            fail(session, "잘못된 요청입니다");
            return true;
    }
}

bool LoginHandler::handleRegister(TlsSession& session,
                                  const HeavenLogin::RegisterRequest& request) {
    const auto* username = request.username();
    const auto* password = request.password();
    const auto* nickname = request.nickname();

    if (username == nullptr || password == nullptr || nickname == nullptr) {
        session.send(proto::encodeRegisterResult(false, "요청에 빠진 항목이 있습니다"));
        session.closeAfterFlush();
        return true;
    }

    const std::string user = username->str();
    const std::string secret = password->str();
    const std::string nick = nickname->str();

    // 형식 검증은 DB 를 건드리기 전에 끝낸다.
    const char* problem = nullptr;
    const auto nickLength = proto::utf8Length(nick);

    if (!proto::isValidUsername(user)) {
        problem = "아이디는 영문, 숫자, 밑줄만 쓸 수 있고 3~32자여야 합니다";
    } else if (secret.size() < proto::kMinPasswordChars ||
               secret.size() > proto::kMaxPasswordBytes) {
        problem = "비밀번호는 8자 이상이어야 합니다";
    } else if (!nickLength.has_value()) {
        problem = "닉네임의 문자 인코딩이 올바르지 않습니다";
    } else if (*nickLength < proto::kMinNicknameChars ||
               *nickLength > proto::kMaxNicknameChars) {
        problem = "닉네임은 2~32자여야 합니다";
    }

    if (problem != nullptr) {
        spdlog::info("register rejected: {} ({}) - {}", user, session.peer(), problem);
        session.send(proto::encodeRegisterResult(false, problem));
        session.closeAfterFlush();
        return true;
    }

    session.markAuthenticated();

    // 해싱이 22ms 라 로그인과 마찬가지로 IOCP 워커에서 하면 안 된다.
    auto self = session.shared_from_this();
    const LoginContext* context = &context_;

    context_.authQueue->submit([self, context, user, secret, nick] {
        std::string hash;
        try {
            hash = hashPassword(secret);
        } catch (const std::exception& e) {
            spdlog::error("failed to hash password for {}: {}", user, e.what());
            self->send(proto::encodeRegisterResult(false, "서버 오류로 가입에 실패했습니다"));
            self->closeAfterFlush();
            return;
        }

        const CreateAccountResult result = context->accounts->createAccount(user, nick, hash);

        switch (result) {
            case CreateAccountResult::Created:
                spdlog::info("registered: {} as '{}' ({})", user, nick, self->peer());
                self->send(proto::encodeRegisterResult(true, "가입이 완료되었습니다"));
                break;
            case CreateAccountResult::UsernameTaken:
                spdlog::info("register rejected: {} - username taken", user);
                self->send(proto::encodeRegisterResult(false, "이미 사용 중인 아이디입니다"));
                break;
            case CreateAccountResult::NicknameTaken:
                spdlog::info("register rejected: {} - nickname '{}' taken", user, nick);
                self->send(proto::encodeRegisterResult(false, "이미 사용 중인 닉네임입니다"));
                break;
            case CreateAccountResult::NotSupported:
                self->send(proto::encodeRegisterResult(
                    false, "이 서버는 가입을 받지 않습니다 (--account-store dev)"));
                break;
            case CreateAccountResult::Error:
                self->send(proto::encodeRegisterResult(false, "서버 오류로 가입에 실패했습니다"));
                break;
        }
        self->closeAfterFlush();
    });

    return true;
}

bool LoginHandler::handleLogin(TlsSession& session, const HeavenLogin::LoginRequest& source) {
    const auto* request = &source;
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
