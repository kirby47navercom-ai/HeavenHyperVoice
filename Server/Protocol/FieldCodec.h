#pragma once

// 필드 메시지 인코딩/검증.

#include <cstdint>
#include <string>
#include <vector>

#include "FieldGeometry.h"
#include "Framing.h"
#include "PokemonSpecies.h"
#include "field_generated.h"

namespace heaven::proto {

// 와이어에 실을 엔티티 하나. nickname 과 partnerSpecies 는 spawned 에서만 채운다.
struct EntityView {
    std::uint64_t entityId = 0;
    float x = 0.f;
    float y = 0.f;
    float facing = 0.f;
    std::string nickname;
    // 둘 다 서버 내부 번호다. 와이어로 나갈 때 도감번호로 바뀐다 (buildEntities).
    std::uint16_t partnerSpecies = 0;
    std::uint16_t species = 0;  // 야생 포켓몬 종족. 0 이면 플레이어.
};

namespace detail {

inline flatbuffers::Offset<
    flatbuffers::Vector<flatbuffers::Offset<HeavenField::EntityState>>>
buildEntities(flatbuffers::FlatBufferBuilder& fbb, const std::vector<EntityView>& entities) {
    std::vector<flatbuffers::Offset<HeavenField::EntityState>> entries;
    entries.reserve(entities.size());

    for (const EntityView& entity : entities) {
        // 문자열은 상위 테이블을 시작하기 전에 만들어야 한다.
        flatbuffers::Offset<flatbuffers::String> nickname = 0;
        if (!entity.nickname.empty()) {
            nickname = fbb.CreateString(entity.nickname);
        }

        HeavenField::EntityStateBuilder builder(fbb);
        builder.add_entity_id(entity.entityId);
        builder.add_x(entity.x);
        builder.add_y(entity.y);
        builder.add_facing(entity.facing);
        if (!nickname.IsNull()) {
            builder.add_nickname(nickname);
        }
        // 내부 번호가 아니라 도감번호를 싣는다. 클라이언트는 이 번호로 에셋을
        // 찾으므로, 종족 표에 한 줄 끼워 넣어도 모델이 밀리지 않는다.
        if (const std::uint16_t dex = dexOf(entity.partnerSpecies); dex != 0) {
            builder.add_partner_species(dex);
        }
        if (const std::uint16_t dex = dexOf(entity.species); dex != 0) {
            builder.add_species(dex);
        }
        entries.push_back(builder.Finish());
    }
    return fbb.CreateVector(entries);
}

inline Bytes wrapField(flatbuffers::FlatBufferBuilder& fbb, HeavenField::Payload type,
                       flatbuffers::Offset<void> payload) {
    fbb.Finish(HeavenField::CreateEnvelope(fbb, type, payload));
    return finishFrame(fbb);
}

}  // namespace detail

// Enter 와 Move 를 만드는 코드는 여기 없다. 서버는 그 둘을 받기만 하고,
// 보내는 쪽은 클라이언트가 자기 인코더를 들고 있다.

// originOffset 은 클라가 좌표를 옮길 때 쓴다 (서버 = 언리얼 + offset).
// roomId 는 인스턴스 서버만 채운다. 필드는 0 이다.
inline Bytes encodeEnterAck(std::uint64_t entityId, float x, float y, float facing,
                            std::uint32_t mapId, float originOffset,
                            std::uint32_t roomId = 0) {
    flatbuffers::FlatBufferBuilder fbb;
    auto ack = HeavenField::CreateEnterAck(fbb, entityId, x, y, facing, mapId, roomId,
                                           originOffset);
    return detail::wrapField(fbb, HeavenField::Payload::EnterAck, ack.Union());
}

inline Bytes encodeSnapshot(const std::vector<EntityView>& spawned,
                            const std::vector<EntityView>& moved,
                            const std::vector<std::uint64_t>& despawned) {
    flatbuffers::FlatBufferBuilder fbb;
    auto spawnedList = detail::buildEntities(fbb, spawned);
    auto movedList = detail::buildEntities(fbb, moved);
    auto despawnedList = fbb.CreateVector(despawned);

    HeavenField::SnapshotBuilder builder(fbb);
    builder.add_spawned(spawnedList);
    builder.add_moved(movedList);
    builder.add_despawned(despawnedList);
    return detail::wrapField(fbb, HeavenField::Payload::Snapshot, builder.Finish().Union());
}

inline Bytes encodeCorrection(std::uint32_t sequence, float x, float y, float facing) {
    flatbuffers::FlatBufferBuilder fbb;
    auto correction = HeavenField::CreateCorrection(fbb, sequence, x, y, facing);
    return detail::wrapField(fbb, HeavenField::Payload::Correction, correction.Union());
}

// 파티 상태. dexNumbers 와 unlocked 는 이미 도감번호다 (저장소가 그렇게 준다).
inline Bytes encodePartyState(bool ok, std::string_view message,
                              const std::vector<std::uint16_t>& dexNumbers,
                              std::uint16_t activeDex,
                              const std::vector<std::uint16_t>& unlocked) {
    flatbuffers::FlatBufferBuilder fbb;
    auto text = fbb.CreateString(message.data(), message.size());
    auto party = fbb.CreateVector(dexNumbers);
    auto candidates = fbb.CreateVector(unlocked);

    HeavenField::PartyStateBuilder builder(fbb);
    builder.add_ok(ok);
    builder.add_message(text);
    builder.add_dex_numbers(party);
    builder.add_active_dex(activeDex);
    builder.add_unlocked(candidates);
    return detail::wrapField(fbb, HeavenField::Payload::PartyState, builder.Finish().Union());
}

// speciesId 는 서버 내부 번호다. 와이어에는 도감번호가 나간다 (EntityState 와 동일).
inline Bytes encodePartnerChanged(std::uint64_t entityId, std::uint16_t speciesId) {
    flatbuffers::FlatBufferBuilder fbb;
    auto changed = HeavenField::CreatePartnerChanged(fbb, entityId, dexOf(speciesId));
    return detail::wrapField(fbb, HeavenField::Payload::PartnerChanged, changed.Union());
}

inline Bytes encodeFieldNotice(std::string_view text) {
    flatbuffers::FlatBufferBuilder fbb;
    auto message = fbb.CreateString(text.data(), text.size());
    auto notice = HeavenField::CreateNotice(fbb, message);
    return detail::wrapField(fbb, HeavenField::Payload::Notice, notice.Union());
}

// 신뢰할 수 없는 입력이므로 GetRoot 전에 반드시 통과시킨다. 실패 시 nullptr.
inline const HeavenField::Envelope* verifyFieldEnvelope(const Bytes& body) {
    flatbuffers::Verifier verifier(body.data(), body.size());
    if (!HeavenField::VerifyEnvelopeBuffer(verifier)) {
        return nullptr;
    }
    const HeavenField::Envelope* envelope = HeavenField::GetEnvelope(body.data());

    // Verifier 는 payload_type 만 있고 payload 오프셋이 없는 프레임을 통과시킨다
    // (VerifyTable(nullptr) 이 true 다). 그대로 두면 payload_as_* 가 nullptr 을
    // 돌려주고, 그것을 역참조하는 호출부가 죽는다.
    return envelope->payload() != nullptr ? envelope : nullptr;
}

}  // namespace heaven::proto
