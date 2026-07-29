#pragma once

// 모든 서버/클라가 공유하는 와이어 프레이밍.
// 프레임 = 4바이트 리틀엔디언 길이 접두사 + FlatBuffer 바디.
//
// 여기에는 스키마에 의존하지 않는 것만 둔다. 메시지별 인코딩은 *Codec.h 로 간다.

#include <flatbuffers/flatbuffers.h>

#include <cstdint>
#include <vector>

namespace heaven::proto {

using Bytes = std::vector<std::uint8_t>;

// 한 프레임의 최대 바디 크기. 길이 접두사만 조작해서 거대한 할당을 유도하는 것을 막는다.
inline constexpr std::uint32_t kMaxBodyBytes = 64u * 1024u;
inline constexpr std::size_t kHeaderBytes = 4;
inline constexpr std::size_t kMaxNicknameBytes = 32;

// FlatBufferBuilder 결과에 길이 접두사를 붙여 전송 가능한 프레임으로 만든다.
inline Bytes finishFrame(flatbuffers::FlatBufferBuilder& fbb) {
    const std::uint32_t len = fbb.GetSize();
    Bytes frame;
    frame.reserve(kHeaderBytes + len);
    frame.push_back(static_cast<std::uint8_t>(len & 0xFFu));
    frame.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFFu));
    frame.push_back(static_cast<std::uint8_t>((len >> 16) & 0xFFu));
    frame.push_back(static_cast<std::uint8_t>((len >> 24) & 0xFFu));
    const std::uint8_t* data = fbb.GetBufferPointer();
    frame.insert(frame.end(), data, data + len);
    return frame;
}

inline std::uint32_t decodeLength(const std::uint8_t* header) {
    return static_cast<std::uint32_t>(header[0]) |
           (static_cast<std::uint32_t>(header[1]) << 8) |
           (static_cast<std::uint32_t>(header[2]) << 16) |
           (static_cast<std::uint32_t>(header[3]) << 24);
}

}  // namespace heaven::proto
