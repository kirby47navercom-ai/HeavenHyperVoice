#pragma once

// 채팅 메시지 인코딩/검증.
// 채팅 프로토콜을 바꿀 때 손대야 할 곳은 chat.fbs 와 이 파일뿐이다.

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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

inline Bytes encodeChat(std::string_view nickname, std::string_view text,
                        HeavenChat::Channel channel = HeavenChat::Channel::General) {
    flatbuffers::FlatBufferBuilder fbb;
    auto nick = fbb.CreateString(nickname.data(), nickname.size());
    auto body = fbb.CreateString(text.data(), text.size());
    auto chat = HeavenChat::CreateChat(fbb, nick, body, channel);
    auto envelope = HeavenChat::CreateEnvelope(fbb, HeavenChat::Payload::Chat, chat.Union());
    fbb.Finish(envelope);
    return finishFrame(fbb);
}

// ------------------------------------------------------------------ 파티

// 파티 한 명. 서버가 채우는 값이라 닉네임은 티켓에서 온 것이다.
struct PartyMemberInfo {
    std::uint64_t accountId = 0;
    std::string nickname;
};

// members 의 순서가 곧 가입 순서이고 첫 번째가 파티장이다.
// partyId 가 0 이면 "파티 없음" 이며 members 는 비어 있다.
inline Bytes encodePartyState(std::uint64_t partyId,
                              const std::vector<PartyMemberInfo>& members,
                              std::string_view message) {
    flatbuffers::FlatBufferBuilder fbb;

    std::vector<flatbuffers::Offset<HeavenChat::PartyMember>> rows;
    rows.reserve(members.size());
    for (const PartyMemberInfo& member : members) {
        auto nick = fbb.CreateString(member.nickname);
        rows.push_back(HeavenChat::CreatePartyMember(fbb, member.accountId, nick));
    }
    auto list = fbb.CreateVector(rows);
    auto text = fbb.CreateString(message.data(), message.size());

    HeavenChat::PartyStateBuilder builder(fbb);
    builder.add_party_id(partyId);
    builder.add_members(list);
    builder.add_message(text);
    auto state = builder.Finish();

    fbb.Finish(HeavenChat::CreateEnvelope(fbb, HeavenChat::Payload::PartyState, state.Union()));
    return finishFrame(fbb);
}

inline Bytes encodePartyInvited(std::uint64_t partyId, std::string_view fromNickname) {
    flatbuffers::FlatBufferBuilder fbb;
    auto nick = fbb.CreateString(fromNickname.data(), fromNickname.size());
    auto invited = HeavenChat::CreatePartyInvited(fbb, partyId, nick);
    fbb.Finish(
        HeavenChat::CreateEnvelope(fbb, HeavenChat::Payload::PartyInvited, invited.Union()));
    return finishFrame(fbb);
}

inline Bytes encodePartyInstanceReady(std::uint32_t instanceType) {
    flatbuffers::FlatBufferBuilder fbb;
    auto ready = HeavenChat::CreatePartyInstanceReady(fbb, instanceType);
    fbb.Finish(HeavenChat::CreateEnvelope(fbb, HeavenChat::Payload::PartyInstanceReady,
                                          ready.Union()));
    return finishFrame(fbb);
}

// 파티 채널 발화. 같은 Chat 표에 채널만 다르게 싣는다. 닉네임에 표시를 섞지
// 않는 이유는, 그러면 클라이언트가 탭으로 거를 수도 색을 달리 줄 수도 없기
// 때문이다 — 화면에 어떻게 그릴지는 클라이언트가 정한다.
inline Bytes encodePartyChat(std::string_view nickname, std::string_view text) {
    return encodeChat(nickname, text, HeavenChat::Channel::Party);
}

// 인스턴스 방 안에서만 오가는 발화.
inline Bytes encodeInstanceChat(std::string_view nickname, std::string_view text) {
    return encodeChat(nickname, text, HeavenChat::Channel::Instance);
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
