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
inline constexpr std::size_t kMaxPasswordBytes = 128;
inline constexpr std::size_t kMaxNicknameBytes = 128;  // UTF-8 32글자 최대치

// DB 는 VARCHAR(32) 로 **글자** 수를 센다. 바이트로 검사하면 한글 닉네임이
// 11글자에서 잘린다(글자당 3바이트). 코드포인트로 세야 스키마와 일치한다.
// 아이디는 ASCII 로 제한되므로 바이트 수가 곧 글자 수다.
inline constexpr std::size_t kMaxUsernameChars = 32;
inline constexpr std::size_t kMinUsernameChars = 3;
inline constexpr std::size_t kMaxNicknameChars = 32;
inline constexpr std::size_t kMinNicknameChars = 2;
inline constexpr std::size_t kMinPasswordChars = 8;

// 저장소와 와이어가 같은 모양을 쓴다. 중간 변환 구조체를 두면 필드를 추가할 때
// 세 곳을 고쳐야 한다.
//
// 지금 데리고 다니는 종족. 개체가 아니라 **종족**이다.
//
// 개체값도 포켓몬별 레벨도 없다. 능력치는 캐릭터 레벨과 종족값으로 그때그때
// 계산하므로, 같은 종족이면 모든 플레이어가 동일하다. 저장되는 것은 "어떤
// 종족을 골랐는가" 뿐이고 나머지는 전부 파생값이다.
struct PartnerInfo {
    std::uint16_t speciesId = 0;   // 서버 내부 번호. 와이어에는 도감번호가 나간다.
    std::string name;              // 종족명. 저장하지 않고 표에서 온다.

    // 캐릭터 레벨과 종족값으로 계산한 결과. 저장소가 읽어올 때 채운다.
    PokemonStats stats;
};

// 캐릭터 외형. 클라이언트의 FUEHHVAppearance 와 1:1 이며, 기본값도 같게 둔다.
// 서버는 내용을 해석하지 않고 범위만 본다 — 어떤 인덱스가 어떤 머리인지는
// 클라이언트의 카탈로그가 안다.
struct AppearanceInfo {
    std::uint8_t gender = 0;  // 0 = TypeA, 1 = TypeB

    std::int32_t body = 1;
    std::int32_t head = 0;
    std::int32_t hair = 0;
    std::int32_t eye = 0;
    std::int32_t equipment = 1;

    float skinR = 1.0f,      skinG = 0.712f,    skinB = 0.6458f;
    float hairR = 0.1719f,   hairG = 0.1111f,   hairB = 0.0850f;
    float eyeR  = 0.070638f, eyeG  = 0.484375f, eyeB  = 0.243701f;

    float armVolume = 0.f;
    float torsoVolume = 0.f;
    float legVolume = 0.f;
};

// 레벨을 갖는 것은 캐릭터다. 포켓몬은 개체가 아니라 종족이라 자기 레벨이 없고,
// 데리고 다니는 캐릭터의 레벨로 능력치가 정해진다.
struct CharacterInfo {
    std::uint64_t id = 0;
    std::string nickname;

    // 포켓몬 능력치의 입력이다. 레벨을 갖는 것은 캐릭터고, 포켓몬은 그 레벨을 쓴다.
    std::uint32_t level = 1;

    // 해금한 것 중 지금 데리고 다니는 종족. 고르지 않았으면 hasPartner 가 false.
    bool hasPartner = false;
    PartnerInfo partner;

    AppearanceInfo appearance;

    // 데리고 다니기로 고른 종족들. 도감번호이고 순서가 슬롯 번호다.
    std::vector<std::uint16_t> party;

    // 해금한 종족 전부. 도감번호. 파티 후보 목록이다.
    std::vector<std::uint16_t> unlocked;
};

// 클라이언트가 보낸 값을 그대로 저장하지 않는다. 인덱스가 음수면 배열 접근이
// 깨지고, 부피가 범위를 벗어나면 모델이 뒤틀린다. 색은 [0,1] 밖이면 발광한다.
//
// 인덱스 상한은 서버가 모른다 — 카탈로그는 클라이언트 에셋이다. 음수만 막고
// 상한은 클라이언트가 표시할 때 클램프한다.
inline void sanitizeAppearance(AppearanceInfo& appearance) {
    const auto clampIndex = [](std::int32_t v) { return v < 0 ? 0 : v; };
    appearance.gender = appearance.gender != 0 ? 1 : 0;
    appearance.body = clampIndex(appearance.body);
    appearance.head = clampIndex(appearance.head);
    appearance.hair = clampIndex(appearance.hair);
    appearance.eye = clampIndex(appearance.eye);
    appearance.equipment = clampIndex(appearance.equipment);

    const auto clampUnit = [](float v) {
        if (!(v >= 0.f)) return 0.f;  // NaN 도 여기서 걸린다
        return v > 1.f ? 1.f : v;
    };
    for (float* channel : {&appearance.skinR, &appearance.skinG, &appearance.skinB,
                           &appearance.hairR, &appearance.hairG, &appearance.hairB,
                           &appearance.eyeR, &appearance.eyeG, &appearance.eyeB}) {
        *channel = clampUnit(*channel);
    }

    const auto clampVolume = [](float v) {
        if (!(v >= -1.f)) return 0.f;  // NaN 은 중립으로
        return v > 1.f ? 1.f : v;
    };
    appearance.armVolume = clampVolume(appearance.armVolume);
    appearance.torsoVolume = clampVolume(appearance.torsoVolume);
    appearance.legVolume = clampVolume(appearance.legVolume);
}

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

// 와이어 -> 구조체. 없으면 기본값 그대로 둔다 (외형을 안 보내는 구버전 클라).
inline AppearanceInfo readAppearance(const HeavenLogin::Appearance* wire) {
    AppearanceInfo out;
    if (wire == nullptr) {
        return out;
    }
    out.gender = wire->gender();
    out.body = wire->body();
    out.head = wire->head();
    out.hair = wire->hair();
    out.eye = wire->eye();
    out.equipment = wire->equipment();
    out.skinR = wire->skin_r();  out.skinG = wire->skin_g();  out.skinB = wire->skin_b();
    out.hairR = wire->hair_r();  out.hairG = wire->hair_g();  out.hairB = wire->hair_b();
    out.eyeR  = wire->eye_r();   out.eyeG  = wire->eye_g();   out.eyeB  = wire->eye_b();
    out.armVolume = wire->arm_volume();
    out.torsoVolume = wire->torso_volume();
    out.legVolume = wire->leg_volume();
    sanitizeAppearance(out);
    return out;
}

// 구조체 -> 와이어. 상위 테이블을 시작하기 전에 불러야 한다.
inline flatbuffers::Offset<HeavenLogin::Appearance> buildAppearance(
    flatbuffers::FlatBufferBuilder& fbb, const AppearanceInfo& appearance) {
    HeavenLogin::AppearanceBuilder builder(fbb);
    builder.add_gender(appearance.gender);
    builder.add_body(appearance.body);
    builder.add_head(appearance.head);
    builder.add_hair(appearance.hair);
    builder.add_eye(appearance.eye);
    builder.add_equipment(appearance.equipment);
    builder.add_skin_r(appearance.skinR);
    builder.add_skin_g(appearance.skinG);
    builder.add_skin_b(appearance.skinB);
    builder.add_hair_r(appearance.hairR);
    builder.add_hair_g(appearance.hairG);
    builder.add_hair_b(appearance.hairB);
    builder.add_eye_r(appearance.eyeR);
    builder.add_eye_g(appearance.eyeG);
    builder.add_eye_b(appearance.eyeB);
    builder.add_arm_volume(appearance.armVolume);
    builder.add_torso_volume(appearance.torsoVolume);
    builder.add_leg_volume(appearance.legVolume);
    return builder.Finish();
}

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
        auto appearance = buildAppearance(fbb, character.appearance);
        auto party = fbb.CreateVector(character.party);
        auto unlocked = fbb.CreateVector(character.unlocked);

        flatbuffers::Offset<HeavenLogin::PokemonSummary> partner = 0;
        if (character.hasPartner) {
            HeavenLogin::PokemonSummaryBuilder builder(fbb);
            builder.add_species_id(character.partner.speciesId);

            // 능력치는 저장된 값이 아니라 캐릭터 레벨과 종족값으로 계산한 결과다.
            builder.add_max_hp(character.partner.stats.maxHp);
            builder.add_atk(character.partner.stats.atk);
            builder.add_def(character.partner.stats.def);
            builder.add_sp_atk(character.partner.stats.spAtk);
            builder.add_sp_def(character.partner.stats.spDef);
            builder.add_speed(character.partner.stats.speed);

            // 클라이언트는 이 번호로 종족 에셋을 찾는다. 표에 없는 종족이면 0 이
            // 나가고, 클라이언트는 파트너를 못 찾은 것으로 처리한다.
            if (const SpeciesBase* base = findSpecies(character.partner.speciesId)) {
                builder.add_dex_number(base->dex);
            }
            partner = builder.Finish();
        }

        HeavenLogin::CharacterSummaryBuilder builder(fbb);
        builder.add_id(character.id);
        builder.add_nickname(nickname);
        builder.add_level(character.level);
        if (!partner.IsNull()) {
            builder.add_partner(partner);
        }
        builder.add_appearance(appearance);
        builder.add_party(party);
        builder.add_unlocked(unlocked);
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

// 요청(클라 -> 서버)을 만드는 코드는 여기 없다. 서버는 받기만 하고, 보내는
// 쪽은 클라이언트가 자기 인코더를 들고 있다.

// ---------------------------------------------------------------- 로그인

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

inline Bytes encodeRegisterResult(bool ok, std::string_view message) {
    flatbuffers::FlatBufferBuilder fbb;
    auto text = fbb.CreateString(message.data(), message.size());

    HeavenLogin::RegisterResponseBuilder builder(fbb);
    builder.add_ok(ok);
    builder.add_message(text);
    return detail::wrap(fbb, HeavenLogin::Payload::RegisterResponse, builder.Finish().Union());
}

// ------------------------------------------------------------- 캐릭터 생성

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

// ------------------------------------------------------------- 캐릭터 선택

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
    const HeavenLogin::Envelope* envelope = HeavenLogin::GetEnvelope(body.data());

    // Verifier 는 payload_type 만 있고 payload 오프셋이 없는 프레임을 통과시킨다
    // (VerifyTable(nullptr) 이 true 다). 그대로 두면 payload_as_* 가 nullptr 을
    // 돌려주고, 그것을 역참조하는 호출부가 죽는다. 여기서 한 번 막으면
    // 모든 호출부가 같이 안전해진다.
    return envelope->payload() != nullptr ? envelope : nullptr;
}

}  // namespace heaven::proto
