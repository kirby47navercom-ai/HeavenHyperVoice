#pragma once

// 로그인 메시지 인코딩/검증.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Framing.h"
#include "PokemonSpecies.h"
#include "login_generated.h"

namespace heaven::proto {

// 프레임 크기를 제한하기 위한 바이트 상한. 내용 규칙은 아래 길이 상수를 쓴다.
inline constexpr std::size_t kMaxUsernameBytes = 32;
inline constexpr std::size_t kMaxPasswordBytes = 128;
inline constexpr std::size_t kMaxNicknameBytes = 128;  // UTF-8 32글자 최대치

// DB 는 VARCHAR(32) 로 **글자** 수를 센다. 바이트로 검사하면 한글 닉네임이
// 11글자에서 잘린다(글자당 3바이트). 코드포인트로 세야 스키마와 일치한다.
inline constexpr std::size_t kMaxUsernameChars = 32;
inline constexpr std::size_t kMinUsernameChars = 3;
inline constexpr std::size_t kMaxNicknameChars = 32;
inline constexpr std::size_t kMinNicknameChars = 2;
inline constexpr std::size_t kMinPasswordChars = 8;

// 저장소와 와이어가 같은 모양을 쓴다. 중간 변환 구조체를 두면 필드를 추가할 때
// 세 곳을 고쳐야 한다.
struct PartnerInfo {
    std::uint16_t speciesId = 0;
    std::string nickname;  // 비어 있으면 종족명을 쓴다
    std::uint32_t level = 0;
    PokemonStats stats;
};

struct CharacterInfo {
    std::uint64_t id = 0;
    std::string nickname;
    std::uint32_t level = 1;

    // 004 이전에 만들어진 캐릭터는 파트너가 없을 수 있다.
    bool hasPartner = false;
    PartnerInfo partner;
};

// UTF-8 코드포인트 개수. 잘못된 바이트열이면 nullopt.
inline std::optional<std::size_t> utf8Length(std::string_view text) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::size_t extra = 0;
        if (lead < 0x80) {
            extra = 0;
        } else if ((lead & 0xE0) == 0xC0) {
            extra = 1;
        } else if ((lead & 0xF0) == 0xE0) {
            extra = 2;
        } else if ((lead & 0xF8) == 0xF0) {
            extra = 3;
        } else {
            return std::nullopt;  // 연속 바이트가 선두에 왔거나 잘못된 선두
        }
        if (i + extra >= text.size()) {
            return std::nullopt;  // 잘린 시퀀스
        }
        for (std::size_t k = 1; k <= extra; ++k) {
            if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) {
                return std::nullopt;
            }
        }
        i += extra + 1;
        ++count;
    }
    return count;
}

// 아이디는 ASCII 영숫자와 밑줄만 받는다. 유니코드를 허용하면 겉보기가 같은
// 다른 글자로 사칭 계정을 만들 수 있다 (호모그래프). 닉네임은 표시용이라 제한하지 않는다.
inline bool isValidUsername(std::string_view username) {
    if (username.size() < kMinUsernameChars || username.size() > kMaxUsernameChars) {
        return false;
    }
    for (const char c : username) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

// 닉네임 형식 검사. 문제가 없으면 nullptr, 있으면 사용자에게 보여줄 사유.
inline const char* validateNickname(std::string_view nickname) {
    const auto length = utf8Length(nickname);
    if (!length.has_value()) {
        return "닉네임의 문자 인코딩이 올바르지 않습니다";
    }
    if (*length < kMinNicknameChars || *length > kMaxNicknameChars) {
        return "닉네임은 2~32자여야 합니다";
    }
    return nullptr;
}

namespace detail {

// CharacterSummary 벡터를 만든다. 응답 세 종류가 같은 목록을 싣는다.
inline flatbuffers::Offset<
    flatbuffers::Vector<flatbuffers::Offset<HeavenLogin::CharacterSummary>>>
buildCharacters(flatbuffers::FlatBufferBuilder& fbb,
                const std::vector<CharacterInfo>& characters) {
    std::vector<flatbuffers::Offset<HeavenLogin::CharacterSummary>> entries;
    entries.reserve(characters.size());

    for (const CharacterInfo& character : characters) {
        // 문자열과 하위 테이블은 상위 테이블을 시작하기 전에 만들어야 한다.
        auto nickname = fbb.CreateString(character.nickname);

        flatbuffers::Offset<HeavenLogin::PokemonSummary> partner = 0;
        if (character.hasPartner) {
            auto partnerNickname = fbb.CreateString(character.partner.nickname);
            HeavenLogin::PokemonSummaryBuilder builder(fbb);
            builder.add_species_id(character.partner.speciesId);
            builder.add_nickname(partnerNickname);
            builder.add_level(character.partner.level);
            builder.add_max_hp(character.partner.stats.maxHp);
            builder.add_atk(character.partner.stats.atk);
            builder.add_def(character.partner.stats.def);
            builder.add_sp_atk(character.partner.stats.spAtk);
            builder.add_sp_def(character.partner.stats.spDef);
            builder.add_speed(character.partner.stats.speed);
            partner = builder.Finish();
        }

        HeavenLogin::CharacterSummaryBuilder builder(fbb);
        builder.add_id(character.id);
        builder.add_nickname(nickname);
        builder.add_level(character.level);
        if (!partner.IsNull()) {
            builder.add_partner(partner);
        }
        entries.push_back(builder.Finish());
    }
    return fbb.CreateVector(entries);
}

inline Bytes wrap(flatbuffers::FlatBufferBuilder& fbb, HeavenLogin::Payload type,
                  flatbuffers::Offset<void> payload) {
    fbb.Finish(HeavenLogin::CreateEnvelope(fbb, type, payload));
    return finishFrame(fbb);
}

}  // namespace detail

// ---------------------------------------------------------------- 로그인

inline Bytes encodeLoginRequest(std::string_view username, std::string_view password) {
    flatbuffers::FlatBufferBuilder fbb;
    auto user = fbb.CreateString(username.data(), username.size());
    auto pass = fbb.CreateString(password.data(), password.size());
    auto request = HeavenLogin::CreateLoginRequest(fbb, user, pass);
    return detail::wrap(fbb, HeavenLogin::Payload::LoginRequest, request.Union());
}

inline Bytes encodeLoginFailure(std::string_view message) {
    flatbuffers::FlatBufferBuilder fbb;
    auto text = fbb.CreateString(message.data(), message.size());

    HeavenLogin::LoginResponseBuilder builder(fbb);
    builder.add_ok(false);
    builder.add_message(text);
    return detail::wrap(fbb, HeavenLogin::Payload::LoginResponse, builder.Finish().Union());
}

// 티켓은 여기서 나오지 않는다. 어느 캐릭터인지 정해져야 발급할 수 있다.
inline Bytes encodeLoginSuccess(const std::vector<CharacterInfo>& characters,
                                std::uint8_t maxSlots) {
    flatbuffers::FlatBufferBuilder fbb;
    auto list = detail::buildCharacters(fbb, characters);

    HeavenLogin::LoginResponseBuilder builder(fbb);
    builder.add_ok(true);
    builder.add_characters(list);
    builder.add_max_slots(maxSlots);
    return detail::wrap(fbb, HeavenLogin::Payload::LoginResponse, builder.Finish().Union());
}

// ---------------------------------------------------------------- 가입

inline Bytes encodeRegisterRequest(std::string_view username, std::string_view password) {
    flatbuffers::FlatBufferBuilder fbb;
    auto user = fbb.CreateString(username.data(), username.size());
    auto pass = fbb.CreateString(password.data(), password.size());
    auto request = HeavenLogin::CreateRegisterRequest(fbb, user, pass);
    return detail::wrap(fbb, HeavenLogin::Payload::RegisterRequest, request.Union());
}

inline Bytes encodeRegisterResult(bool ok, std::string_view message) {
    flatbuffers::FlatBufferBuilder fbb;
    auto text = fbb.CreateString(message.data(), message.size());

    HeavenLogin::RegisterResponseBuilder builder(fbb);
    builder.add_ok(ok);
    builder.add_message(text);
    return detail::wrap(fbb, HeavenLogin::Payload::RegisterResponse, builder.Finish().Union());
}

// ------------------------------------------------------------- 캐릭터 생성

inline Bytes encodeCreateCharacterRequest(std::string_view nickname, std::uint16_t speciesId) {
    flatbuffers::FlatBufferBuilder fbb;
    auto nick = fbb.CreateString(nickname.data(), nickname.size());
    auto request = HeavenLogin::CreateCreateCharacterRequest(fbb, nick, speciesId);
    return detail::wrap(fbb, HeavenLogin::Payload::CreateCharacterRequest, request.Union());
}

// 생성/삭제/방생이 모두 이 응답을 쓴다. 셋 다 목록이 바뀌는 일이다.
inline Bytes encodeCharacterList(bool ok, std::string_view message,
                                 const std::vector<CharacterInfo>& characters) {
    flatbuffers::FlatBufferBuilder fbb;
    auto text = fbb.CreateString(message.data(), message.size());
    auto list = detail::buildCharacters(fbb, characters);

    HeavenLogin::CharacterListResponseBuilder builder(fbb);
    builder.add_ok(ok);
    builder.add_message(text);
    builder.add_characters(list);
    return detail::wrap(fbb, HeavenLogin::Payload::CharacterListResponse,
                        builder.Finish().Union());
}

inline Bytes encodeDeleteCharacterRequest(std::uint64_t characterId,
                                          std::string_view confirmNickname) {
    flatbuffers::FlatBufferBuilder fbb;
    auto nick = fbb.CreateString(confirmNickname.data(), confirmNickname.size());
    auto request = HeavenLogin::CreateDeleteCharacterRequest(fbb, characterId, nick);
    return detail::wrap(fbb, HeavenLogin::Payload::DeleteCharacterRequest, request.Union());
}

inline Bytes encodeReleasePartnerRequest(std::uint64_t characterId) {
    flatbuffers::FlatBufferBuilder fbb;
    auto request = HeavenLogin::CreateReleasePartnerRequest(fbb, characterId);
    return detail::wrap(fbb, HeavenLogin::Payload::ReleasePartnerRequest, request.Union());
}

// ------------------------------------------------------------- 캐릭터 선택

inline Bytes encodeSelectCharacterRequest(std::uint64_t characterId) {
    flatbuffers::FlatBufferBuilder fbb;
    auto request = HeavenLogin::CreateSelectCharacterRequest(fbb, characterId);
    return detail::wrap(fbb, HeavenLogin::Payload::SelectCharacterRequest, request.Union());
}

inline Bytes encodeSelectCharacterFailure(std::string_view message) {
    flatbuffers::FlatBufferBuilder fbb;
    auto text = fbb.CreateString(message.data(), message.size());

    HeavenLogin::SelectCharacterResponseBuilder builder(fbb);
    builder.add_ok(false);
    builder.add_message(text);
    return detail::wrap(fbb, HeavenLogin::Payload::SelectCharacterResponse,
                        builder.Finish().Union());
}

// 서비스 하나에 붙는 데 필요한 것. 티켓은 이미 그 service 의 audience 로
// 서명돼 있어야 한다.
struct EndpointInfo {
    std::string service;
    std::string host;
    std::uint16_t port = 0;
    Bytes ticket;
};

inline Bytes encodeSelectCharacterSuccess(const std::vector<EndpointInfo>& endpoints,
                                          std::string_view nickname) {
    flatbuffers::FlatBufferBuilder fbb;

    std::vector<flatbuffers::Offset<HeavenLogin::ServiceEndpoint>> entries;
    entries.reserve(endpoints.size());
    for (const EndpointInfo& endpoint : endpoints) {
        auto service = fbb.CreateString(endpoint.service);
        auto host = fbb.CreateString(endpoint.host);
        auto blob = fbb.CreateVector(endpoint.ticket);

        HeavenLogin::ServiceEndpointBuilder builder(fbb);
        builder.add_service(service);
        builder.add_host(host);
        builder.add_port(endpoint.port);
        builder.add_ticket(blob);
        entries.push_back(builder.Finish());
    }
    auto list = fbb.CreateVector(entries);
    auto nick = fbb.CreateString(nickname.data(), nickname.size());

    HeavenLogin::SelectCharacterResponseBuilder builder(fbb);
    builder.add_ok(true);
    builder.add_endpoints(list);
    builder.add_nickname(nick);
    return detail::wrap(fbb, HeavenLogin::Payload::SelectCharacterResponse,
                        builder.Finish().Union());
}

// 신뢰할 수 없는 입력이므로 GetRoot 전에 반드시 통과시킨다. 실패 시 nullptr.
inline const HeavenLogin::Envelope* verifyLoginEnvelope(const Bytes& body) {
    flatbuffers::Verifier verifier(body.data(), body.size());
    if (!HeavenLogin::VerifyEnvelopeBuffer(verifier)) {
        return nullptr;
    }
    return HeavenLogin::GetEnvelope(body.data());
}

}  // namespace heaven::proto
