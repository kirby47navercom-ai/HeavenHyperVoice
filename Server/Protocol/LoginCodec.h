#pragma once

// 로그인 메시지 인코딩/검증.

#include <optional>
#include <string>
#include <string_view>

#include "Framing.h"
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

inline Bytes encodeLoginRequest(std::string_view username, std::string_view password) {
    flatbuffers::FlatBufferBuilder fbb;
    auto user = fbb.CreateString(username.data(), username.size());
    auto pass = fbb.CreateString(password.data(), password.size());
    auto request = HeavenLogin::CreateLoginRequest(fbb, user, pass);
    auto envelope =
        HeavenLogin::CreateEnvelope(fbb, HeavenLogin::Payload::LoginRequest, request.Union());
    fbb.Finish(envelope);
    return finishFrame(fbb);
}

inline Bytes encodeLoginFailure(std::string_view message) {
    flatbuffers::FlatBufferBuilder fbb;
    auto text = fbb.CreateString(message.data(), message.size());

    HeavenLogin::LoginResponseBuilder builder(fbb);
    builder.add_ok(false);
    builder.add_message(text);
    auto response = builder.Finish();

    auto envelope =
        HeavenLogin::CreateEnvelope(fbb, HeavenLogin::Payload::LoginResponse, response.Union());
    fbb.Finish(envelope);
    return finishFrame(fbb);
}

inline Bytes encodeLoginSuccess(const Bytes& ticket, std::string_view chatHost,
                                std::uint16_t chatPort, std::string_view nickname) {
    flatbuffers::FlatBufferBuilder fbb;
    auto blob = fbb.CreateVector(ticket);
    auto host = fbb.CreateString(chatHost.data(), chatHost.size());
    auto nick = fbb.CreateString(nickname.data(), nickname.size());

    HeavenLogin::LoginResponseBuilder builder(fbb);
    builder.add_ok(true);
    builder.add_ticket(blob);
    builder.add_chat_host(host);
    builder.add_chat_port(chatPort);
    builder.add_nickname(nick);
    auto response = builder.Finish();

    auto envelope =
        HeavenLogin::CreateEnvelope(fbb, HeavenLogin::Payload::LoginResponse, response.Union());
    fbb.Finish(envelope);
    return finishFrame(fbb);
}

inline Bytes encodeRegisterRequest(std::string_view username, std::string_view password,
                                   std::string_view nickname) {
    flatbuffers::FlatBufferBuilder fbb;
    auto user = fbb.CreateString(username.data(), username.size());
    auto pass = fbb.CreateString(password.data(), password.size());
    auto nick = fbb.CreateString(nickname.data(), nickname.size());
    auto request = HeavenLogin::CreateRegisterRequest(fbb, user, pass, nick);
    auto envelope =
        HeavenLogin::CreateEnvelope(fbb, HeavenLogin::Payload::RegisterRequest, request.Union());
    fbb.Finish(envelope);
    return finishFrame(fbb);
}

inline Bytes encodeRegisterResult(bool ok, std::string_view message) {
    flatbuffers::FlatBufferBuilder fbb;
    auto text = fbb.CreateString(message.data(), message.size());

    HeavenLogin::RegisterResponseBuilder builder(fbb);
    builder.add_ok(ok);
    builder.add_message(text);
    auto response = builder.Finish();

    auto envelope =
        HeavenLogin::CreateEnvelope(fbb, HeavenLogin::Payload::RegisterResponse, response.Union());
    fbb.Finish(envelope);
    return finishFrame(fbb);
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
