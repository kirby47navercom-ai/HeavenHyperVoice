#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <string_view>

#include "AccountStore.h"
#include "AuthTicket.h"
#include "CharacterStore.h"
#include "FrameHandler.h"
#include "LoginCodec.h"
#include "TlsSession.h"
#include "WorkQueue.h"

namespace heaven::login {

using net::FrameHandler;
using net::TlsSession;
using net::WorkQueue;

// 저장소는 Data 라이브러리로 나갔다. FieldServer 도 같은 것을 쓴다.
using data::Account;
using data::AccountStore;
using data::Character;
using data::CharacterStore;
using data::CreateAccountResult;
using data::CreateCharacterResult;
using data::kMaxCharactersPerAccount;

// 클라이언트가 붙어야 하는 서비스 하나. 티켓은 service 이름을 audience 로
// 삼아 따로 서명되므로 필드 티켓을 채팅 서버에 낼 수 없다.
struct ServiceTarget {
    std::string service;  // proto::kAudienceField / kAudienceChat
    std::string host;
    std::uint16_t port = 0;
};

// 티켓 발급에 필요한, 세션마다 바뀌지 않는 것들.
struct LoginContext {
    AccountStore* accounts = nullptr;
    CharacterStore* characters = nullptr;
    const proto::TicketSigner* signer = nullptr;
    WorkQueue* authQueue = nullptr;

    // 캐릭터를 고르면 이 전부에 대해 티켓이 한 장씩 발급된다.
    std::vector<ServiceTarget> targets;

    std::int64_t ticketTtlSeconds = 60;
    std::string issuer;
};

// 세션 하나의 로그인 처리.
//
//   AwaitingLogin      LoginRequest 나 RegisterRequest 를 기다린다
//   AwaitingSelection  인증됨. 캐릭터를 만들거나 고를 수 있다
//   Done               응답을 보냈고 연결을 닫는 중이다
//
// 티켓은 Done 으로 넘어갈 때만 나간다. 어느 캐릭터로 들어가는지 정해져야
// 발급할 수 있기 때문이다.
//
// onFrame 은 파싱만 하고 즉시 반환한다. 계정/캐릭터 조회와 비밀번호 검증은
// 인증 스레드에서 돌아가며, 응답도 거기서 보낸다.
class LoginHandler : public FrameHandler {
public:
    explicit LoginHandler(const LoginContext& context) : context_(context) {}

    bool onFrame(TlsSession& session, const proto::Bytes& body) override;

private:
    // Busy 는 인증 스레드가 요청을 처리하는 동안이다. 클라이언트가 응답을
    // 기다리지 않고 다음 프레임을 보내면 여기서 걸린다.
    enum class Stage { AwaitingLogin, Busy, AwaitingSelection, Done };

    bool handleLogin(TlsSession& session, const HeavenLogin::LoginRequest& request);
    bool handleRegister(TlsSession& session, const HeavenLogin::RegisterRequest& request);
    bool handleCreateCharacter(TlsSession& session,
                               const HeavenLogin::CreateCharacterRequest& request);
    bool handleSelectCharacter(TlsSession& session,
                               const HeavenLogin::SelectCharacterRequest& request);

    // 응답을 보내고 연결을 닫는다. 인증 스레드에서도 불린다.
    void finish(TlsSession& session, const proto::Bytes& frame);
    void fail(TlsSession& session, std::string_view message);
    // 다음 요청을 받을 수 있게 되돌린다 (연결은 유지).
    void resume(Stage stage);

    const LoginContext& context_;

    // 이 핸들러의 상태는 IOCP 워커와 인증 스레드가 같이 만진다. 다른 핸들러는
    // onFrame 이 한 스레드에서만 불려 락이 필요 없지만, 여기는 응답을 인증
    // 스레드가 보내므로 예외다.
    std::mutex mutex_;
    Stage stage_ = Stage::AwaitingLogin;

    // 인증이 끝난 뒤에만 채워진다. 캐릭터 조회는 반드시 이 값으로 좁힌다 —
    // 클라이언트가 보낸 character_id 를 그대로 믿으면 남의 캐릭터로 티켓이 나간다.
    std::uint64_t accountId_ = 0;
    std::string username_;
};

}  // namespace heaven::login
