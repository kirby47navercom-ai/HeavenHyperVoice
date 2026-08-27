#include "AuthTicket.h"

#include <openssl/err.h>
#include <openssl/pem.h>

#include <stdexcept>
#include <utility>

#include "OpenSslError.h"
#include "auth_generated.h"

namespace heaven::proto {

namespace {

EVP_PKEY* loadKey(const std::string& path, bool isPrivate) {
    BIO* bio = BIO_new_file(path.c_str(), "r");
    if (bio == nullptr) {
        throw std::runtime_error("cannot open key file " + path + ": " + opensslError());
    }

    EVP_PKEY* key = isPrivate ? PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr)
                              : PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (key == nullptr) {
        throw std::runtime_error("cannot parse key file " + path + ": " + opensslError());
    }
    if (EVP_PKEY_base_id(key) != EVP_PKEY_ED25519) {
        EVP_PKEY_free(key);
        throw std::runtime_error(path + " is not an Ed25519 key");
    }
    return key;
}

}  // namespace

const char* describe(TicketError error) {
    switch (error) {
        case TicketError::Ok:            return "ok";
        case TicketError::Malformed:     return "malformed ticket";
        case TicketError::MissingFields: return "ticket is missing required claims";
        case TicketError::UnknownKey:    return "unknown signing key";
        case TicketError::BadSignature:  return "signature verification failed";
        case TicketError::WrongAudience: return "ticket was issued for another service";
        case TicketError::Expired:       return "ticket expired";
        case TicketError::NotYetValid:   return "ticket is not valid yet";
    }
    return "unknown error";
}

// ---------------------------------------------------------- TicketSigner

TicketSigner::TicketSigner(const std::string& privateKeyPath, std::string keyId)
    : key_(loadKey(privateKeyPath, /*isPrivate=*/true)), keyId_(std::move(keyId)) {}

TicketSigner::~TicketSigner() {
    if (key_ != nullptr) {
        EVP_PKEY_free(key_);
    }
}

Bytes TicketSigner::sign(const TicketClaims& claims) const {
    flatbuffers::FlatBufferBuilder fbb;
    auto issuer = fbb.CreateString(claims.issuer);
    auto audience = fbb.CreateString(claims.audience);
    auto keyId = fbb.CreateString(keyId_);
    auto nickname = fbb.CreateString(claims.nickname);

    HeavenAuth::AuthTicketBuilder builder(fbb);
    builder.add_issuer(issuer);
    builder.add_audience(audience);
    builder.add_key_id(keyId);
    builder.add_account_id(claims.accountId);
    builder.add_character_id(claims.characterId);
    builder.add_nickname(nickname);
    builder.add_issued_unix(claims.issuedUnix);
    builder.add_expires_unix(claims.expiresUnix);
    fbb.Finish(builder.Finish());

    const std::uint8_t* payload = fbb.GetBufferPointer();
    const std::size_t payloadSize = fbb.GetSize();

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        throw std::runtime_error("EVP_MD_CTX_new failed: " + opensslError());
    }

    // Ed25519 는 다이제스트를 따로 주지 않고 한 번에 서명한다.
    if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key_) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestSignInit failed: " + opensslError());
    }

    std::size_t signatureSize = kEd25519SignatureBytes;
    Bytes ticket(payloadSize + kEd25519SignatureBytes);
    std::copy(payload, payload + payloadSize, ticket.begin());

    if (EVP_DigestSign(ctx, ticket.data() + payloadSize, &signatureSize, payload, payloadSize) !=
        1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestSign failed: " + opensslError());
    }
    EVP_MD_CTX_free(ctx);

    if (signatureSize != kEd25519SignatureBytes) {
        throw std::runtime_error("unexpected Ed25519 signature size");
    }
    return ticket;
}

// --------------------------------------------------------- PublicKeyRing

PublicKeyRing::~PublicKeyRing() {
    for (auto& [id, key] : keys_) {
        EVP_PKEY_free(key);
    }
}

void PublicKeyRing::add(const std::string& keyId, const std::string& publicKeyPath) {
    EVP_PKEY* key = loadKey(publicKeyPath, /*isPrivate=*/false);
    auto [it, inserted] = keys_.emplace(keyId, key);
    if (!inserted) {
        EVP_PKEY_free(key);
        throw std::runtime_error("duplicate key id: " + keyId);
    }
}

EVP_PKEY* PublicKeyRing::find(std::string_view keyId) const {
    const auto it = keys_.find(keyId);
    return it == keys_.end() ? nullptr : it->second;
}

// ----------------------------------------------------------- verifyTicket

TicketError verifyTicket(const Bytes& ticket, std::string_view expectedAudience,
                         const PublicKeyRing& keys, std::int64_t nowUnix, VerifiedTicket& out) {
    if (ticket.size() <= kEd25519SignatureBytes) {
        return TicketError::Malformed;
    }

    const std::size_t payloadSize = ticket.size() - kEd25519SignatureBytes;
    const std::uint8_t* payload = ticket.data();
    const std::uint8_t* signature = ticket.data() + payloadSize;

    // 서명을 확인하기 전에 key_id 를 읽어야 하므로, 먼저 구조를 검증한다.
    // 신뢰할 수 없는 입력이므로 Verifier 통과 없이는 어떤 필드도 읽지 않는다.
    flatbuffers::Verifier verifier(payload, payloadSize);
    if (!HeavenAuth::VerifyAuthTicketBuffer(verifier)) {
        return TicketError::Malformed;
    }
    const auto* parsed = HeavenAuth::GetAuthTicket(payload);

    if (parsed->key_id() == nullptr || parsed->audience() == nullptr ||
        parsed->nickname() == nullptr || parsed->nickname()->size() == 0) {
        return TicketError::MissingFields;
    }

    EVP_PKEY* key = keys.find(parsed->key_id()->string_view());
    if (key == nullptr) {
        return TicketError::UnknownKey;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        return TicketError::BadSignature;
    }
    bool signatureOk = false;
    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1) {
        signatureOk = EVP_DigestVerify(ctx, signature, kEd25519SignatureBytes, payload,
                                       payloadSize) == 1;
    }
    EVP_MD_CTX_free(ctx);
    ERR_clear_error();

    if (!signatureOk) {
        return TicketError::BadSignature;
    }

    // 서명이 확인된 뒤에야 클레임을 신뢰한다.
    if (parsed->audience()->string_view() != expectedAudience) {
        return TicketError::WrongAudience;
    }
    if (nowUnix >= parsed->expires_unix()) {
        return TicketError::Expired;
    }
    // 시계 오차를 조금 허용한다.
    if (nowUnix + 60 < parsed->issued_unix()) {
        return TicketError::NotYetValid;
    }

    out.accountId = parsed->account_id();
    out.characterId = parsed->character_id();
    out.nickname = parsed->nickname()->str();
    out.issuer = parsed->issuer() != nullptr ? parsed->issuer()->str() : std::string();
    return TicketError::Ok;
}

}  // namespace heaven::proto
