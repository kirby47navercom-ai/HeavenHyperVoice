#pragma once

// 계정 조회 인터페이스.
//
// 구현체는 OdbcAccountStore(실제 DB)와 DevAccountStore(개발용)가 있다.
// 다른 저장소로 갈아끼워도 LoginHandler 는 바뀌지 않는다.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace heaven::login {

struct Account {
    std::uint64_t id = 0;
    std::string nickname;
};

enum class CreateAccountResult {
    Created,
    UsernameTaken,
    NicknameTaken,
    NotSupported,  // 저장소가 계정 생성을 지원하지 않는다
    Error,
};

const char* describe(CreateAccountResult result);

class AccountStore {
public:
    virtual ~AccountStore() = default;

    // 새 계정을 만든다. 비밀번호는 이미 해싱된 상태로 받는다 — 해싱 정책은
    // 저장소가 알 바가 아니고, 호출자가 인증 스레드에서 미리 처리한다.
    virtual CreateAccountResult createAccount(std::string_view username,
                                              std::string_view nickname,
                                              const std::string& passwordHash) = 0;

    // 자격증명이 맞으면 계정을, 아니면 nullopt.
    //
    // 인증 스레드 여럿에서 동시에 불리므로 구현체는 스레드 안전해야 한다.
    // 느려도 되는 자리다 — IOCP 워커는 여기까지 오지 않는다 (WorkQueue 참고).
    virtual std::optional<Account> authenticate(std::string_view username,
                                                std::string_view password) = 0;

    // 기동 로그에 찍을 이름.
    virtual const char* describe() const = 0;

    // 가입을 받을 수 있는 상태인지. 권한이 없으면 로그인만 받고 가입은 거절한다.
    virtual bool supportsRegistration() const = 0;
};

}  // namespace heaven::login
