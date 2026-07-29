#pragma once

// 채팅 메시지 인코딩/검증.
// 채팅 프로토콜을 바꿀 때 손대야 할 곳은 chat.fbs 와 이 파일뿐이다.

#include <string>
#include <string_view>

#include "Framing.h"
#include "chat_generated.h"

namespace heaven::proto {

// 닉네임은 티켓 안에 있다. 클라이언트가 스스로 주장하지 않는다.
inline Bytes encodeHello(const Bytes& ticket) {
    flatbuffers::FlatBufferBuilder fbb;
    auto blob = fbb.CreateVector(ticket);
    auto hello = HeavenChat::CreateHello(fbb, blob);
    auto envelope = HeavenChat::CreateEnvelope(fbb, HeavenChat::Payload::Hello, hello.Union());
    fbb.Finish(envelope);
    return finishFrame(fbb);
}

inline Bytes encodeSay(std::string_view text) {
    flatbuffers::FlatBufferBuilder fbb;
    auto body = fbb.CreateString(text.data(), text.size());
    auto say = HeavenChat::CreateSay(fbb, body);
    auto envelope = HeavenChat::CreateEnvelope(fbb, HeavenChat::Payload::Say, say.Union());
    fbb.Finish(envelope);
    return finishFrame(fbb);
}

inline Bytes encodeNotice(std::string_view text) {
    flatbuffers::FlatBufferBuilder fbb;
    auto body = fbb.CreateString(text.data(), text.size());
    auto notice = HeavenChat::CreateNotice(fbb, body);
    auto envelope = HeavenChat::CreateEnvelope(fbb, HeavenChat::Payload::Notice, notice.Union());
    fbb.Finish(envelope);
    return finishFrame(fbb);
}

inline Bytes encodeChat(std::string_view nickname, std::string_view text) {
    flatbuffers::FlatBufferBuilder fbb;
    auto nick = fbb.CreateString(nickname.data(), nickname.size());
    auto body = fbb.CreateString(text.data(), text.size());
    auto chat = HeavenChat::CreateChat(fbb, nick, body);
    auto envelope = HeavenChat::CreateEnvelope(fbb, HeavenChat::Payload::Chat, chat.Union());
    fbb.Finish(envelope);
    return finishFrame(fbb);
}

// 네트워크에서 받은 바디를 검증한다. 신뢰할 수 없는 입력이므로 GetRoot 전에 반드시 통과시킨다.
// 실패 시 nullptr.
inline const HeavenChat::Envelope* verifyEnvelope(const Bytes& body) {
    flatbuffers::Verifier verifier(body.data(), body.size());
    if (!HeavenChat::VerifyEnvelopeBuffer(verifier)) {
        return nullptr;
    }
    return HeavenChat::GetEnvelope(body.data());
}

}  // namespace heaven::proto
