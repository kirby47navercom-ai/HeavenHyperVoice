#pragma once

// 필드 메시지 인코딩/검증.

#include <cstdint>
#include <chrono>
#include <string>
#include <vector>

#include "FieldGeometry.h"
#include "Appearance.h"
#include "Framing.h"
#include "GachaPool.h"
#include "PokemonSpecies.h"
#include "field_generated.h"

namespace heaven::proto {

// 와이어에 실을 엔티티 하나. nickname 과 partnerSpecies 는 spawned 에서만 채운다.
struct EntityView {
    std::uint64_t entityId = 0;
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float velocityX = 0.f;
    float velocityY = 0.f;
    float velocityZ = 0.f;
    float facing = 0.f;
    bool hasPartnerTransform = false;
    float partnerX = 0.f;
    float partnerY = 0.f;
    float partnerZ = 0.f;
    float partnerVelocityX = 0.f;
    float partnerVelocityY = 0.f;
    float partnerVelocityZ = 0.f;
    float partnerFacing = 0.f;
    bool partnerTeleported = false;
    std::string nickname;
    // 둘 다 서버 내부 번호다. 와이어로 나갈 때 도감번호로 바뀐다 (buildEntities).
    std::uint16_t partnerSpecies = 0;
    std::uint16_t species = 0;  // 야생 포켓몬 종족. 0 이면 플레이어.
    std::uint32_t attackSequence = 0;
    std::uint64_t attackTargetId = 0;

    // 야생 포켓몬의 현재/최대 체력. InstanceServer만 채운다.
    std::uint16_t currentHp = 0;
    std::uint16_t maxHp = 0;

    // 플레이어 외형. hasAppearance 가 false 면 와이어에 넣지 않는다.
    // moved 목록과 야생 포켓몬이 그렇다 — 전자는 안 변하는 값을 매 틱 보내지
    // 않으려는 것이고, 후자는 받는 쪽에서 읽지도 않기 때문이다.
    bool hasAppearance = false;
    AppearanceInfo appearance;
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

        // 하위 테이블도 상위 테이블을 시작하기 전에 만들어야 한다.
        flatbuffers::Offset<HeavenField::Appearance> appearance = 0;
        if (entity.hasAppearance) {
            const AppearanceInfo& look = entity.appearance;
            appearance = HeavenField::CreateAppearance(
                fbb, look.gender, look.body, look.head, look.hair, look.eye, look.equipment,
                look.skinR, look.skinG, look.skinB, look.hairR, look.hairG, look.hairB,
                look.eyeR, look.eyeG, look.eyeB, look.armVolume, look.torsoVolume,
                look.legVolume);
        }

        HeavenField::EntityStateBuilder builder(fbb);
        builder.add_entity_id(entity.entityId);
        builder.add_x(entity.x);
        builder.add_y(entity.y);
        builder.add_z(entity.z);
        builder.add_velocity_x(entity.velocityX);
        builder.add_velocity_y(entity.velocityY);
        builder.add_velocity_z(entity.velocityZ);
        builder.add_facing(entity.facing);
        if (entity.hasPartnerTransform) {
            builder.add_partner_present(true);
            builder.add_partner_x(entity.partnerX);
            builder.add_partner_y(entity.partnerY);
            builder.add_partner_z(entity.partnerZ);
            builder.add_partner_velocity_x(entity.partnerVelocityX);
            builder.add_partner_velocity_y(entity.partnerVelocityY);
            builder.add_partner_velocity_z(entity.partnerVelocityZ);
            builder.add_partner_facing(entity.partnerFacing);
            builder.add_partner_teleported(entity.partnerTeleported);
        }
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
        if (entity.attackSequence != 0) {
            builder.add_attack_sequence(entity.attackSequence);
            builder.add_attack_target_id(entity.attackTargetId);
        }
        if (entity.maxHp != 0) {
            builder.add_current_hp(entity.currentHp);
            builder.add_max_hp(entity.maxHp);
        }
        if (!appearance.IsNull()) {
            builder.add_appearance(appearance);
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
inline Bytes encodeEnterAck(std::uint64_t entityId, float x, float y, float z, float facing,
                            std::uint32_t mapId, float originOffset,
                            std::uint32_t roomId = 0) {
    flatbuffers::FlatBufferBuilder fbb;
    auto ack = HeavenField::CreateEnterAck(fbb, entityId, x, y, facing, mapId, roomId,
                                           originOffset, z);
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
    builder.add_server_time_seconds(std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    return detail::wrapField(fbb, HeavenField::Payload::Snapshot, builder.Finish().Union());
}

inline Bytes encodeCorrection(std::uint32_t sequence, float x, float y, float z, float facing) {
    flatbuffers::FlatBufferBuilder fbb;
    auto correction = HeavenField::CreateCorrection(fbb, sequence, x, y, facing, z);
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

// 뽑기 결과. dex 는 이미 도감번호다 (추첨표가 도감번호로 적혀 있다) —
// EntityState 처럼 내부 번호를 바꿔 실을 것이 없다.
inline Bytes encodeGachaResult(bool ok, std::string_view message, std::uint16_t dex,
                               GachaRarity rarity, bool duplicate) {
    flatbuffers::FlatBufferBuilder fbb;
    auto text = fbb.CreateString(message.data(), message.size());
    auto result = HeavenField::CreateGachaDrawResponse(
        fbb, ok, text, dex, static_cast<std::uint8_t>(rarity), duplicate);
    return detail::wrapField(fbb, HeavenField::Payload::GachaDrawResponse, result.Union());
}

inline Bytes encodeTokenBalance(std::uint32_t tokens) {
    flatbuffers::FlatBufferBuilder fbb;
    auto balance = HeavenField::CreateTokenBalance(fbb, tokens);
    return detail::wrapField(fbb, HeavenField::Payload::TokenBalance, balance.Union());
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
