#pragma once

// 채팅 메시지 인코딩/검증.
// 채팅 프로토콜을 바꿀 때 손대야 할 곳은 chat.fbs 와 이 파일뿐이다.

#include <chrono>
#include <string>
#include <string_view>

#include "Framing.h"
#include "chat_generated.h"

namespace heaven::proto {

// 한 발화의 상한. 프레임 상한(64KiB)에만 기대면 한 사람이 보낸 64KiB 가
// 접속자 수만큼 증폭돼 나간다.
inline constexpr std::size_t kMaxChatTextBytes = 1024;

// 발화 사이 최소 간격. 사람이 치는 속도보다 넉넉하다.
inline constexpr std::chrono::milliseconds kMinSayInterval{200};

// Hello 와 Say 를 만드는 코드는 여기 없다. 서버는 그 둘을 받기만 하고,
// 보내는 쪽은 클라이언트가 자기 인코더를 들고 있다. 닉네임은 티켓 안에 있어서
// 어차피 클라이언트가 주장할 수 있는 값이 아니다.

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
    const HeavenChat::Envelope* envelope = HeavenChat::GetEnvelope(body.data());

    // Verifier 는 payload_type 만 있고 payload 오프셋이 없는 프레임을 통과시킨다
    // (VerifyTable(nullptr) 이 true 다). 그대로 두면 payload_as_* 가 nullptr 을
    // 돌려주고, 그것을 역참조하는 호출부가 죽는다.
    return envelope->payload() != nullptr ? envelope : nullptr;
}

}  // namespace heaven::proto
