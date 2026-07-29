#pragma once

// 계정 조회 인터페이스.
//
// DB 를 붙일 자리다. 지금은 DevAccountStore 하나뿐이며,
// 나중에 SqlAccountStore 등을 여기에 끼워 넣으면 LoginHandler 는 바뀌지 않는다.

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
    // 여러 IOCP 워커 스레드에서 동시에 불릴 수 있으므로 구현체는 스레드 안전해야 한다.
    virtual std::optional<Account> authenticate(std::string_view username,
                                                std::string_view password) = 0;

    // 기동 로그에 찍을 이름.
    virtual const char* describe() const = 0;
};

}  // namespace heaven::login
