#pragma once

#include "AccountStore.h"

namespace heaven::login {

// 개발용 임시 구현. **자격증명을 실제로 검증하지 않는다.**
//
// 비어 있지 않은 아이디면 통과시키고, 아이디에서 결정적으로 계정 ID 를 만든다.
// 같은 아이디로 다시 로그인하면 같은 계정 ID 가 나온다.
//
// DB 없이 채팅 흐름만 시험할 때 쓴다 (--account-store dev). 기동 시 경고를 남긴다.
class DevAccountStore : public AccountStore {
public:
    std::optional<Account> authenticate(std::string_view username,
                                        std::string_view password) override;

    // 저장할 곳이 없다. 어차피 아무 아이디나 로그인되므로 가입도 의미가 없다.
    CreateAccountResult createAccount(std::string_view username, std::string_view nickname,
                                      const std::string& passwordHash) override;

    const char* describe() const override { return "DevAccountStore (no real authentication)"; }

    bool supportsRegistration() const override { return false; }
};

}  // namespace heaven::login
