#include "PasswordHash.h"

#include <argon2.h>
#include <openssl/rand.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace heaven::data {

namespace {

// OWASP 권장 최소치.
constexpr std::uint32_t kMemoryKiB = 19456;  // 19 MiB
constexpr std::uint32_t kIterations = 2;
constexpr std::uint32_t kParallelism = 1;
constexpr std::uint32_t kSaltBytes = 16;
constexpr std::uint32_t kHashBytes = 32;

}  // namespace

std::string hashPassword(std::string_view password) {
    unsigned char salt[kSaltBytes];
    if (RAND_bytes(salt, static_cast<int>(sizeof(salt))) != 1) {
        throw std::runtime_error("failed to generate a password salt");
    }

    const std::size_t capacity = argon2_encodedlen(kIterations, kMemoryKiB, kParallelism,
                                                   kSaltBytes, kHashBytes, Argon2_id);

    // argon2 는 NUL 종단 문자열을 쓰므로 capacity 에 NUL 자리가 포함돼 있다.
    std::vector<char> buffer(capacity);
    const int rc =
        argon2id_hash_encoded(kIterations, kMemoryKiB, kParallelism, password.data(),
                              password.size(), salt, sizeof(salt), kHashBytes, buffer.data(),
                              buffer.size());
    if (rc != ARGON2_OK) {
        throw std::runtime_error(std::string("argon2id hashing failed: ") +
                                 argon2_error_message(rc));
    }

    return std::string(buffer.data(), std::strlen(buffer.data()));
}

bool verifyPassword(std::string_view password, const std::string& encoded) {
    if (encoded.empty()) {
        return false;
    }
    return argon2id_verify(encoded.c_str(), password.data(), password.size()) == ARGON2_OK;
}

}  // namespace heaven::data
