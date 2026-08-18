#pragma once

// 필드 메시지 인코딩/검증.

#include <cstdint>
#include <string>
#include <vector>

#include "FieldGeometry.h"
#include "Framing.h"
#include "field_generated.h"

namespace heaven::proto {

// 와이어에 실을 엔티티 하나. nickname 과 partnerSpecies 는 spawned 에서만 채운다.
struct EntityView {
    std::uint64_t entityId = 0;
    float x = 0.f;
    float y = 0.f;
    float facing = 0.f;
    std::string nickname;
    std::uint16_t partnerSpecies = 0;
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
        if (entity.partnerSpecies != 0) {
            builder.add_partner_species(entity.partnerSpecies);
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

inline Bytes encodeEnter(const Bytes& ticket) {
    flatbuffers::FlatBufferBuilder fbb;
    auto blob = fbb.CreateVector(ticket);
    auto request = HeavenField::CreateEnter(fbb, blob);
    return detail::wrapField(fbb, HeavenField::Payload::Enter, request.Union());
}

// --dev-no-auth 로 뜬 서버에만 통한다.
inline Bytes encodeDevEnter(std::string_view name, std::uint64_t characterId,
                            std::uint16_t partnerSpecies) {
    flatbuffers::FlatBufferBuilder fbb;
    auto devName = fbb.CreateString(name.data(), name.size());

    HeavenField::EnterBuilder builder(fbb);
    builder.add_dev_name(devName);
    builder.add_dev_character_id(characterId);
    builder.add_dev_partner_species(partnerSpecies);
    return detail::wrapField(fbb, HeavenField::Payload::Enter, builder.Finish().Union());
}

inline Bytes encodeEnterAck(std::uint64_t entityId, float x, float y, float facing,
                            std::uint32_t mapId) {
    flatbuffers::FlatBufferBuilder fbb;
    auto ack = HeavenField::CreateEnterAck(fbb, entityId, x, y, facing, mapId);
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
    return HeavenField::GetEnvelope(body.data());
}

}  // namespace heaven::proto
