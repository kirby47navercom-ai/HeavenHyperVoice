#pragma once

// 인증 티켓 발급/검증.
//
// 발급은 개인키를 가진 쪽(LoginServer)만 할 수 있고, 검증은 공개키만 있으면 된다.
// 덕분에 채팅/음성/게이트웨이는 티켓을 확인할 수는 있어도 만들어낼 수는 없다.
// 나중에 발급을 별도 서비스로 빼내도 검증하는 쪽 코드는 바뀌지 않는다.

#include <openssl/evp.h>

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

#include "Framing.h"

namespace heaven::proto {

inline constexpr std::size_t kEd25519SignatureBytes = 64;

// 티켓의 issued/expires 와 verifyTicket 의 now 가 쓰는 시각.
//
// 발급하는 쪽과 검증하는 세 서버가 각자 같은 함수를 들고 있었다. 벽시계를
// 쓰는 이유는 티켓이 프로세스 경계를 넘기 때문이다 — steady_clock 은 프로세스
// 마다 기준점이 달라 만료를 비교할 수 없다.
inline std::int64_t nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// 서비스별 audience 값. 음성/게이트웨이가 생기면 여기에 추가한다.
//
// 검증하는 쪽이 자기 값과 일치하는지 반드시 확인한다. 이게 없으면 필드 티켓을
// 채팅 서버에, 채팅 티켓을 음성 서버에 재사용할 수 있다.
inline constexpr std::string_view kAudienceChat = "chat";
inline constexpr std::string_view kAudienceField = "field";
inline constexpr std::string_view kAudienceInstance = "instance";

struct TicketClaims {
    std::string issuer;
    std::string audience;
    std::uint64_t accountId = 0;
    std::uint64_t characterId = 0;
    std::string nickname;
    std::int64_t issuedUnix = 0;
    std::int64_t expiresUnix = 0;
};

struct VerifiedTicket {
    std::uint64_t accountId = 0;
    std::uint64_t characterId = 0;
    std::string nickname;
    std::string issuer;
};

enum class TicketError {
    Ok,
    Malformed,      // 길이가 안 맞거나 FlatBuffer 검증 실패
    MissingFields,  // 필수 클레임이 비었다
    UnknownKey,     // key_id 가 키링에 없다
    BadSignature,
    WrongAudience,
    Expired,
    NotYetValid,
};

const char* describe(TicketError error);

// 개인키 보유자만 만들 수 있다. LoginServer 전용.
class TicketSigner {
public:
    // privateKeyPath: PEM 형식 Ed25519 개인키
    TicketSigner(const std::string& privateKeyPath, std::string keyId);
    ~TicketSigner();

    TicketSigner(const TicketSigner&) = delete;
    TicketSigner& operator=(const TicketSigner&) = delete;

    // [AuthTicket FlatBuffer][64바이트 서명] 을 만든다.
    Bytes sign(const TicketClaims& claims) const;

private:
    EVP_PKEY* key_ = nullptr;
    std::string keyId_;
};

// key_id -> 공개키. 지금은 항목이 하나지만 무중단 키 교체 경로를 열어둔다.
class PublicKeyRing {
public:
    PublicKeyRing() = default;
    ~PublicKeyRing();

    PublicKeyRing(const PublicKeyRing&) = delete;
    PublicKeyRing& operator=(const PublicKeyRing&) = delete;

    // publicKeyPath: PEM 형식 Ed25519 공개키
    void add(const std::string& keyId, const std::string& publicKeyPath);

    EVP_PKEY* find(std::string_view keyId) const;

private:
    std::map<std::string, EVP_PKEY*, std::less<>> keys_;
};

// expectedAudience 를 인자로 강제해 호출자가 확인을 빼먹을 수 없게 한다.
TicketError verifyTicket(const Bytes& ticket, std::string_view expectedAudience,
                         const PublicKeyRing& keys, std::int64_t nowUnix, VerifiedTicket& out);

}  // namespace heaven::proto
