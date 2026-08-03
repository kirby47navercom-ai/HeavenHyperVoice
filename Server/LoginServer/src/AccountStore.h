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

class AccountStore {
public:
    virtual ~AccountStore() = default;

    // 자격증명이 맞으면 계정을, 아니면 nullopt.
    //
    // 인증 스레드 여럿에서 동시에 불리므로 구현체는 스레드 안전해야 한다.
    // 느려도 되는 자리다 — IOCP 워커는 여기까지 오지 않는다 (WorkQueue 참고).
    virtual std::optional<Account> authenticate(std::string_view username,
                                                std::string_view password) = 0;

    // 기동 로그에 찍을 이름.
    virtual const char* describe() const = 0;
};

}  // namespace heaven::login
