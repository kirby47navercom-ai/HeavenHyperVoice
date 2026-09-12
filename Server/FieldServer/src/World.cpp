#include "World.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace heaven::field {

namespace {

constexpr float kEnterRadiusSquared = proto::kEnterRadius * proto::kEnterRadius;
constexpr float kExitRadiusSquared = proto::kExitRadius * proto::kExitRadius;

fieldshared::PartnerOwnerState partnerOwnerStateOf(const Entity &entity) {
    return {
        {entity.position.x, entity.position.y, entity.position.z},
        {entity.velocityX, entity.velocityY, entity.velocityZ},
        entity.position.facing,
    };
}

} // namespace

proto::EntityView World::viewOf(const Entity &entity, bool withIdentity) {
    proto::EntityView view;
    view.movement = entity.movement.state;
    view.partnerMovement = entity.partner.movement;
    view.entityId = entity.characterId;
    view.x = entity.position.x;
    view.y = entity.position.y;
    view.z = entity.position.z;
    view.velocityX = entity.velocityX;
    view.velocityY = entity.velocityY;
    view.velocityZ = entity.velocityZ;
    view.facing = entity.position.facing;
    if (entity.partnerSpecies != 0 && entity.partner.initialized) {
        view.hasPartnerTransform = true;
        view.partnerX = entity.partner.location.x;
        view.partnerY = entity.partner.location.y;
        view.partnerZ = entity.partner.location.z;
        view.partnerVelocityX = entity.partner.velocity.x;
        view.partnerVelocityY = entity.partner.velocity.y;
        view.partnerVelocityZ = entity.partner.velocity.z;
        view.partnerFacing = entity.partner.facing;
        view.partnerTeleported = entity.partner.teleportedThisTick;
    }
    if (withIdentity) {
        view.nickname = entity.nickname;
        view.partnerSpecies = entity.partnerSpecies;
        view.hasAppearance = true;
        view.appearance = entity.appearance;
    }
    return view;
}

void World::sendTo(const Entity &entity, const proto::Bytes &frame) const {
    // TlsSession::send 는 스레드 안전하고 월드 상태를 만지지 않는다.
    // 그래서 락을 쥔 채로 불러도 순서가 뒤집히지 않는다.
    if (const auto session = entity.session.lock()) {
        session->send(frame);
    }
}

Position World::resolvePosition(const Position &position) const {
    Position resolved = position;
    resolved.x = proto::clampToWorld(resolved.x);
    resolved.y = proto::clampToWorld(resolved.y);

    if (map_ != nullptr && map_->loaded()) {
        nav::Vec3 grounded;
        if (map_->canStandAt(resolved.x, resolved.y, map_->agent(), &grounded, resolved.z)) {
            resolved.x = grounded.x;
            resolved.y = grounded.y;
            resolved.z = grounded.z;
        } else if (map_->canStandAt(proto::kSpawnX, proto::kSpawnY, map_->agent(), &grounded)) {
            // 이전 맵에 저장된 위치가 건물이나 바다가 되었으면 중앙 시작점으로 복귀한다.
            resolved.x = grounded.x;
            resolved.y = grounded.y;
            resolved.z = grounded.z;
        }
    }
    return resolved;
}

Displaced World::enter(std::uint64_t characterId, std::uint64_t accountId, std::string nickname,
                       std::uint16_t partnerSpecies, const proto::AppearanceInfo &appearance,
                       const Position &position, const std::shared_ptr<TlsSession> &session) {
    std::lock_guard<std::mutex> lock(mutex_);

    Displaced displaced;
    if (const auto it = byAccount_.find(accountId); it != byAccount_.end()) {
        const std::uint64_t previous = it->second;
        if (const auto entity = entities_.find(previous); entity != entities_.end()) {
            displaced.session = entity->second.session.lock();
            // 밀려난 쪽의 onClosed 는 이 자리를 못 찾아 leave() 가 헛돈다.
            // 마지막 위치를 여기서 건네지 않으면 저장할 기회가 아예 없다.
            displaced.characterId = previous;
            displaced.position = entity->second.position;
            removeFromVisibility(entity->second);
            sectors_[static_cast<std::size_t>(entity->second.sector)].erase(previous);
            entities_.erase(entity);
        }
        byAccount_.erase(it);
    }

    Entity entity;
    entity.characterId = characterId;
    entity.accountId = accountId;
    entity.session = session;
    entity.nickname = std::move(nickname);
    entity.partnerSpecies = partnerSpecies;
    entity.appearance = appearance;
    entity.mapId = position.mapId;
    entity.position = resolvePosition(position);
    entity.sector = proto::sectorIndex(entity.position.x, entity.position.y);
    hhv::movement::State initial;
    initial.position = {entity.position.x - (proto::kWorldSize / 2.f),
                        entity.position.y - (proto::kWorldSize / 2.f), entity.position.z};
    initial.facing = entity.position.facing;
    entity.movement.reset(initial);
    if (entity.partnerSpecies != 0) {
        fieldshared::PartnerFollower::initialize(partnerOwnerStateOf(entity), entity.partnerSpecies,
                                                 entity.partner, map_);
    }

    // 번호가 이미 있으면 emplace 는 아무것도 넣지 않고 기존 것을 가리킨다.
    // 그대로 진행하면 남의 엔티티를 자기 것인 양 만지게 된다.
    auto [inserted, ok] = entities_.emplace(characterId, std::move(entity));
    if (!ok) {
        spdlog::error("entity {} already in world, refusing entry", characterId);
        return displaced;
    }

    sectors_[static_cast<std::size_t>(inserted->second.sector)].insert(characterId);
    byAccount_[accountId] = characterId;

    updateVisibility(inserted->second);
    return displaced;
}

std::optional<Position> World::leave(std::uint64_t characterId, const TlsSession *session) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = entities_.find(characterId);
    if (it == entities_.end()) {
        return std::nullopt;
    }

    // 같은 캐릭터로 재접속하면 새 세션이 이 자리를 이미 차지한 뒤다. 번호만
    // 보고 지우면 방금 들어온 살아 있는 쪽을 월드에서 떼어내 버린다.
    if (it->second.session.lock().get() != session) {
        return std::nullopt;
    }

    const Position position = it->second.position;
    removeFromVisibility(it->second);
    sectors_[static_cast<std::size_t>(it->second.sector)].erase(characterId);

    if (const auto owner = byAccount_.find(it->second.accountId);
        owner != byAccount_.end() && owner->second == characterId) {
        byAccount_.erase(owner);
    }
    entities_.erase(it);
    return position;
}

void World::removeFromVisibility(Entity &self) {
    const std::vector<std::uint64_t> gone{self.visible.begin(), self.visible.end()};

    // 받는 사람마다 같은 바이트열이다. 한 번만 만든다.
    const proto::Bytes despawn = proto::encodeSnapshot({}, {}, {self.characterId});

    for (const std::uint64_t otherId : gone) {
        const auto other = entities_.find(otherId);
        if (other == entities_.end()) {
            continue;
        }
        other->second.visible.erase(self.characterId);
        sendTo(other->second, despawn);
    }
    self.visible.clear();
}

void World::updateVisibility(Entity &self) {
    // 1) 시야에서 나간 것부터 정리한다. 섹터를 보지 않고 거리로만 판정하므로
    //    순간이동으로 후보 밖까지 튄 경우도 여기서 걸린다.
    for (auto it = self.visible.begin(); it != self.visible.end();) {
        const auto other = entities_.find(*it);
        if (other == entities_.end()) {
            it = self.visible.erase(it);
            continue;
        }

        const float d2 = proto::distanceSquared(self.position.x, self.position.y, other->second.position.x,
                                                other->second.position.y);
        if (d2 <= kExitRadiusSquared && other->second.mapId == self.mapId) {
            ++it;
            continue;
        }

        other->second.visible.erase(self.characterId);
        sendTo(other->second, proto::encodeSnapshot({}, {}, {self.characterId}));
        sendTo(self, proto::encodeSnapshot({}, {}, {other->second.characterId}));
        it = self.visible.erase(it);
    }

    // 2) 새로 들어온 것. 후보는 3×3 섹터이며, kEnterRadius <= kSectorSize 라
    //    이 범위가 시야를 반드시 덮는다 (FieldGeometry.h 의 static_assert).
    proto::forEachNeighborSector(self.sector, [&](int sector) {
        for (const std::uint64_t otherId : sectors_[static_cast<std::size_t>(sector)]) {
            if (otherId == self.characterId || self.visible.count(otherId) != 0) {
                continue;
            }
            const auto other = entities_.find(otherId);
            if (other == entities_.end()) {
                continue;
            }

            // 섹터 격자는 맵을 구분하지 않는다. 같은 좌표의 다른 맵(동굴 등)이
            // 서로 보이지 않게 여기서 거른다.
            if (other->second.mapId != self.mapId) {
                continue;
            }

            const float d2 = proto::distanceSquared(self.position.x, self.position.y,
                                                    other->second.position.x, other->second.position.y);
            if (d2 > kEnterRadiusSquared) {
                continue;
            }

            // 양쪽 모두 갱신한다. A 가 움직이면 "A 가 보는 것" 뿐 아니라
            // "B 가 A 를 보는 것" 도 같이 바뀐다.
            self.visible.insert(otherId);
            other->second.visible.insert(self.characterId);

            sendTo(self, proto::encodeSnapshot({viewOf(other->second, true)}, {}, {}));
            sendTo(other->second, proto::encodeSnapshot({viewOf(self, true)}, {}, {}));
        }
    });
}

void World::setPartnerSpecies(std::uint64_t characterId, std::uint16_t partnerSpecies) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = entities_.find(characterId);
    if (it == entities_.end()) {
        return; // 아직 안 들어왔거나 이미 나갔다
    }

    Entity &entity = it->second;
    if (entity.partnerSpecies == partnerSpecies) {
        return; // 같은 값이면 알릴 것이 없다
    }
    entity.partnerSpecies = partnerSpecies;
    fieldshared::PartnerFollower::reset(entity.partner);
    if (entity.partnerSpecies != 0) {
        fieldshared::PartnerFollower::initialize(partnerOwnerStateOf(entity), entity.partnerSpecies,
                                                 entity.partner, map_);
    }

    // 프레임은 한 번만 만들어 돌려 쓴다. 보는 사람 수만큼 직렬화할 이유가 없다.
    const proto::Bytes frame = proto::encodePartnerChanged(characterId, partnerSpecies);
    for (const std::uint64_t viewerId : entity.visible) {
        if (const auto viewer = entities_.find(viewerId); viewer != entities_.end()) {
            sendTo(viewer->second, frame);
        }
    }
    sendTo(entity, frame);
}

bool World::move(std::uint64_t characterId, const TlsSession *session,
                 const std::vector<hhv::movement::PredictedInput> &inputs) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entities_.find(characterId);
    if (found == entities_.end() || found->second.session.lock().get() != session || map_ == nullptr ||
        !map_->loaded()) {
        return false;
    }
    return found->second.movement.enqueue(inputs);
}

void World::advancePlayers(float dt) {
    if (map_ == nullptr || !map_->loaded()) {
        return;
    }

    for (auto &[characterId, entity] : entities_) {
        if (entity.session.expired() ||
            !entity.movement.advance(dt, hhv::movement::Config{}, map_->collision())) {
            continue;
        }

        const auto &state = entity.movement.state;
        const auto location = map_->toServer(state.position);
        entity.position.x = location.x;
        entity.position.y = location.y;
        entity.position.z = location.z;
        entity.position.facing = state.facing;
        entity.velocityX = state.velocity.x;
        entity.velocityY = state.velocity.y;
        entity.velocityZ = state.velocity.z;
        entity.movedThisTick = true;
        sendTo(entity, proto::encodeCorrection(entity.movement.acknowledged, state));

        const int sector = proto::sectorIndex(location.x, location.y);
        if (sector != entity.sector) {
            sectors_[static_cast<std::size_t>(entity.sector)].erase(characterId);
            sectors_[static_cast<std::size_t>(sector)].insert(characterId);
            entity.sector = sector;
        }
        updateVisibility(entity);
    }
}

void World::advancePartners(float dt) {
    for (auto &[characterId, entity] : entities_) {
        (void)characterId;
        const bool pendingMove = entity.partner.movedThisTick;
        const bool pendingTeleport = entity.partner.teleportedThisTick;
        fieldshared::PartnerFollower::update(dt, partnerOwnerStateOf(entity), entity.partnerSpecies,
                                             entity.partner, map_);
        entity.partner.movedThisTick = entity.partner.movedThisTick || pendingMove;
        entity.partner.teleportedThisTick = entity.partner.teleportedThisTick || pendingTeleport;
    }
}

void World::tick(float dt) {
    std::lock_guard<std::mutex> lock(mutex_);
    advancePlayers(dt);
    advancePartners(dt);

    // 뷰어별로 모은다. 시야 집합이 대칭이라 "나를 보는 사람" = visible 이다.
    //
    // ponytail: 뷰어마다 프레임을 따로 만든다. 틱당 Σ|visible| 만큼 직렬화하는데,
    // 수천 명 규모가 되면 델타 압축이나 관심도별 갱신 빈도 조절이 필요하다.
    // 섹터 단위로 뭉쳐 공유 버퍼를 쓸 수도 있지만, 그러면 시야 밖 좌표가
    // 클라에 흘러가서 뷰 리스트를 둔 이유가 없어진다.
    std::unordered_map<std::uint64_t, std::vector<proto::EntityView>> pending;

    for (auto &[characterId, entity] : entities_) {
        const bool partnerMoved = entity.partner.movedThisTick;
        if (!entity.movedThisTick && !partnerMoved) {
            continue;
        }

        const proto::EntityView view = viewOf(entity, /*withIdentity=*/false);
        entity.movedThisTick = false;
        entity.partner.movedThisTick = false;
        entity.partner.teleportedThisTick = false;
        if (partnerMoved) {
            pending[characterId].push_back(view);
        }
        for (const std::uint64_t viewerId : entity.visible) {
            pending[viewerId].push_back(view);
        }
    }

    for (const auto &[viewerId, moved] : pending) {
        const auto viewer = entities_.find(viewerId);
        if (viewer != entities_.end()) {
            sendTo(viewer->second, proto::encodeSnapshot({}, moved, {}));
        }
    }
}

std::vector<std::pair<std::uint64_t, Position>> World::positions() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<std::uint64_t, Position>> out;
    out.reserve(entities_.size());
    for (const auto &[characterId, entity] : entities_) {
        out.emplace_back(characterId, entity.position);
    }
    return out;
}

std::size_t World::size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return entities_.size();
}

} // namespace heaven::field
