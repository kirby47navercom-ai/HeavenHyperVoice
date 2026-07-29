#pragma once

#include "AccountStore.h"

namespace heaven::login {

// 개발용 임시 구현. **자격증명을 실제로 검증하지 않는다.**
//
// 비어 있지 않은 아이디면 통과시키고, 아이디에서 결정적으로 계정 ID 를 만든다.
// 같은 아이디로 다시 로그인하면 같은 계정 ID 가 나온다.
//
// DB 가 붙기 전까지의 자리 채움이며, 기동 시 경고를 남긴다.
class DevAccountStore : public AccountStore {
public:
    std::optional<Account> authenticate(std::string_view username,
                                        std::string_view password) override;

    const char* describe() const override { return "DevAccountStore (no real authentication)"; }
};

}  // namespace heaven::login
