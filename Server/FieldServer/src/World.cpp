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

void World::move(std::uint64_t characterId, float x, float y, float facing,
                 std::uint32_t sequence) {
    // NaN/Inf 를 먼저 막는다. clampToWorld 의 비교는 NaN 에 대해 모두 거짓이라
    // 그대로 통과하고, sectorIndex 의 float->int 변환이 정의되지 않은 값을 내며
    // sectors_ 를 배열 밖에서 건드리게 된다.
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(facing)) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = entities_.find(characterId);
    if (it == entities_.end()) {
        return;
    }
    Entity& self = it->second;

    // 좌표는 클램프해도 **빈도**는 막지 못한다. 회선 속도로 밀어 넣으면 월드
    // 전역 락과 시야 재계산을 독점할 수 있어, 너무 잦은 것은 그냥 버린다.
    // lastMoveAt 을 갱신하지 않으므로 속도 예산은 그대로 쌓인다.
    const auto now = std::chrono::steady_clock::now();
    if (now - self.lastMoveAt < proto::kMinMoveInterval) {
        return;
    }

    x = proto::clampToWorld(x);
    y = proto::clampToWorld(y);

    // 속도 상한. 거절이 아니라 클램프다 — 랙 스파이크로 정상 유저를 튕기지 않는다.
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

    // 벽 검사. 도착점만 보면 한 틱에 캡슐 지름보다 멀리 움직일 때 얇은 벽을
    // 그냥 지나간다. 출발점부터 훑는다.
    bool corrected = distance > allowed && distance > 0.f;
    if (collision_ != nullptr) {
        // 바닥이 아직 없어서 캡슐 높이를 고정으로 둔다. 하이트맵이 들어오면
        // 여기가 floor.z + halfHeight 가 된다.
        const float z = agent_.capsuleHalfHeight;
        const Vec3 from{self.position.x, self.position.y, z};
        const Vec3 to{x, y, z};

        if (collision_->blockedAlong(from, to, agent_)) {
            // 통과시키지 않고 제자리에 둔다. 밀어내기(슬라이딩)는 클라이언트
            // 물리가 이미 하므로, 서버는 "거기 못 간다" 만 말하면 된다.
            x = self.position.x;
            y = self.position.y;
            corrected = true;
            spdlog::debug("{} blocked by wall at ({:.0f}, {:.0f})", self.nickname, to.x, to.y);
        }
    }

    self.position.x = x;
    self.position.y = y;
    self.position.facing = facing;
    self.lastMoveAt = now;
    self.movedThisTick = true;

    // 서버가 좌표를 고쳤을 때만 알린다. 정상 이동까지 응답하면 20Hz x 접속자
    // 만큼 왕복이 생긴다. 클라는 이 sequence 부터 다시 예측한다.
    if (corrected) {
        sendTo(self, proto::encodeCorrection(sequence, x, y, facing));
    }

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
