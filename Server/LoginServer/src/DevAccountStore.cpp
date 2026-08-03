#include "DevAccountStore.h"

namespace heaven::login {

// 비밀번호는 받지도 않는다. 아이디 형식 검증은 LoginHandler 가 이미 했다.
std::optional<Account> DevAccountStore::authenticate(std::string_view username,
                                                     std::string_view) {
    if (username.empty()) {
        return std::nullopt;
    }

    // 아이디에서 결정적으로 계정 ID 를 만든다 (FNV-1a).
    // 같은 아이디는 항상 같은 ID 를 받는다.
    std::uint64_t hash = 1469598103934665603ull;
    for (const char c : username) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }

    Account account;
    account.id = hash;
    account.nickname = std::string(username);
    return account;
}

}  // namespace heaven::login
