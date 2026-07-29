#pragma once

// 로그인 메시지 인코딩/검증.

#include <string>
#include <string_view>

#include "Framing.h"
#include "login_generated.h"

namespace heaven::proto {

inline constexpr std::size_t kMaxUsernameBytes = 32;
inline constexpr std::size_t kMaxPasswordBytes = 128;

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

// 신뢰할 수 없는 입력이므로 GetRoot 전에 반드시 통과시킨다. 실패 시 nullptr.
inline const HeavenLogin::Envelope* verifyLoginEnvelope(const Bytes& body) {
    flatbuffers::Verifier verifier(body.data(), body.size());
    if (!HeavenLogin::VerifyEnvelopeBuffer(verifier)) {
        return nullptr;
    }
    return HeavenLogin::GetEnvelope(body.data());
}

}  // namespace heaven::proto
