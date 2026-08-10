#include "World.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <utility>

namespace heaven::field {

namespace {

constexpr float kEnterRadiusSquared = proto::kEnterRadius * proto::kEnterRadius;
constexpr float kExitRadiusSquared = proto::kExitRadius * proto::kExitRadius;

}  // namespace

proto::EntityView World::viewOf(const Entity& entity, bool withIdentity) {
    proto::EntityView view;
    view.entityId = entity.characterId;
    view.x = entity.position.x;
    view.y = entity.position.y;
    view.facing = entity.position.facing;
    if (withIdentity) {
        view.nickname = entity.nickname;
        view.partnerSpecies = entity.partnerSpecies;
    }
    return view;
}

void World::sendTo(const Entity& entity, const proto::Bytes& frame) const {
    // TlsSession::send 는 스레드 안전하고 월드 상태를 만지지 않는다.
    // 그래서 락을 쥔 채로 불러도 순서가 뒤집히지 않는다.
    if (const auto session = entity.session.lock()) {
        session->send(frame);
    }
}

std::shared_ptr<TlsSession> World::enter(std::uint64_t characterId, std::uint64_t accountId,
                                         std::string nickname, std::uint16_t partnerSpecies,
                                         const Position& position,
                                         const std::shared_ptr<TlsSession>& session) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::shared_ptr<TlsSession> displaced;
    if (const auto it = byAccount_.find(accountId); it != byAccount_.end()) {
        const std::uint64_t previous = it->second;
        if (const auto entity = entities_.find(previous); entity != entities_.end()) {
            displaced = entity->second.session.lock();
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
    entity.position = position;
    entity.position.x = proto::clampToWorld(entity.position.x);
    entity.position.y = proto::clampToWorld(entity.position.y);
    entity.sector = proto::sectorIndex(entity.position.x, entity.position.y);
    entity.lastMoveAt = std::chrono::steady_clock::now();

    sectors_[static_cast<std::size_t>(entity.sector)].insert(characterId);
    byAccount_[accountId] = characterId;
    auto [inserted, ok] = entities_.emplace(characterId, std::move(entity));
    (void)ok;

    updateVisibility(inserted->second);
    return displaced;
}

std::optional<Position> World::leave(std::uint64_t characterId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = entities_.find(characterId);
    if (it == entities_.end()) {
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

void World::removeFromVisibility(Entity& self) {
    const std::vector<std::uint64_t> gone{self.visible.begin(), self.visible.end()};
    const std::vector<std::uint64_t> despawned{self.characterId};

    for (const std::uint64_t otherId : gone) {
        const auto other = entities_.find(otherId);
        if (other == entities_.end()) {
            continue;
        }
        other->second.visible.erase(self.characterId);
        sendTo(other->second, proto::encodeSnapshot({}, {}, despawned));
    }
    self.visible.clear();
}

void World::updateVisibility(Entity& self) {
    // 1) 시야에서 나간 것부터 정리한다. 섹터를 보지 않고 거리로만 판정하므로
    //    순간이동으로 후보 밖까지 튄 경우도 여기서 걸린다.
    for (auto it = self.visible.begin(); it != self.visible.end();) {
        const auto other = entities_.find(*it);
        if (other == entities_.end()) {
            it = self.visible.erase(it);
            continue;
        }

        const float d2 = proto::distanceSquared(self.position.x, self.position.y,
                                                other->second.position.x,
                                                other->second.position.y);
        if (d2 <= kExitRadiusSquared) {
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

            const float d2 = proto::distanceSquared(self.position.x, self.position.y,
                                                    other->second.position.x,
                                                    other->second.position.y);
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

void World::move(std::uint64_t characterId, float x, float y, float facing) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = entities_.find(characterId);
    if (it == entities_.end()) {
        return;
    }
    Entity& self = it->second;

    x = proto::clampToWorld(x);
    y = proto::clampToWorld(y);

    // 속도 상한. 거절이 아니라 클램프다 — 랙 스파이크로 정상 유저를 튕기지 않는다.
    const auto now = std::chrono::steady_clock::now();
    const float elapsed = std::chrono::duration<float>(now - self.lastMoveAt).count();
    const float allowed = proto::kMaxSpeed * elapsed + proto::kSpeedSlack;
    const float distance =
        std::sqrt(proto::distanceSquared(self.position.x, self.position.y, x, y));

    if (distance > allowed && distance > 0.f) {
        const float scale = allowed / distance;
        x = self.position.x + (x - self.position.x) * scale;
        y = self.position.y + (y - self.position.y) * scale;
        // 치터는 이걸 초당 수십 번 만든다. warn 으로 올리면 로그가 잠긴다.
        spdlog::debug("{} moved {:.0f}uu in {:.0f}ms, clamped to {:.0f}uu", self.nickname,
                      distance, elapsed * 1000.f, allowed);
    }

    self.position.x = x;
    self.position.y = y;
    self.position.facing = facing;
    self.lastMoveAt = now;
    self.movedThisTick = true;

    const int sector = proto::sectorIndex(x, y);
    if (sector != self.sector) {
        sectors_[static_cast<std::size_t>(self.sector)].erase(characterId);
        sectors_[static_cast<std::size_t>(sector)].insert(characterId);
        self.sector = sector;
    }

    // Spawn/Despawn 은 틱을 기다리지 않는다. Move 가 Spawn 보다 먼저 도착하면
    // 클라가 모르는 엔티티의 좌표를 받게 된다.
    updateVisibility(self);
}

void World::tick() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 뷰어별로 모은다. 시야 집합이 대칭이라 "나를 보는 사람" = visible 이다.
    //
    // ponytail: 뷰어마다 프레임을 따로 만든다. 틱당 Σ|visible| 만큼 직렬화하는데,
    // 수천 명 규모가 되면 델타 압축이나 관심도별 갱신 빈도 조절이 필요하다.
    // 섹터 단위로 뭉쳐 공유 버퍼를 쓸 수도 있지만, 그러면 시야 밖 좌표가
    // 클라에 흘러가서 뷰 리스트를 둔 이유가 없어진다.
    std::unordered_map<std::uint64_t, std::vector<proto::EntityView>> pending;

    for (auto& [characterId, entity] : entities_) {
        if (!entity.movedThisTick) {
            continue;
        }
        entity.movedThisTick = false;

        const proto::EntityView view = viewOf(entity, /*withIdentity=*/false);
        for (const std::uint64_t viewerId : entity.visible) {
            pending[viewerId].push_back(view);
        }
    }

    for (const auto& [viewerId, moved] : pending) {
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
    for (const auto& [characterId, entity] : entities_) {
        out.emplace_back(characterId, entity.position);
    }
    return out;
}

std::size_t World::size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return entities_.size();
}

}  // namespace heaven::field
