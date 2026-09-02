#include "RoomManager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <map>
#include <random>
#include <utility>

#include "InstanceGeometry.h"
#include "PokemonSpecies.h"

namespace heaven::instance {

namespace {

// 맵이 없을 때 야생을 뿌릴 상자의 반폭.
constexpr float kDefaultWildHalfExtent = 4000.f;

// 맵 경계에 딱 붙여 뿌리면 첫 배회에서 바로 지형 밖을 노리게 되므로 안쪽으로 민다.
constexpr float kWildAreaFraction = 0.8f;

// 야생을 뿌리고 배회시킬 구역. 맵 경계를 따라간다 — 스폰 상자와 배회 상자가
// 어긋나면 스폰된 자리에서 구역 안으로 걸어 들어가느라 처음 몇 초가 어색해지고,
// 맵과도 안 맞으면 아예 지형 밖에 뜬다.
WildArea wildAreaFor(const Map* map) {
    WildArea area;
    if (map != nullptr && map->loaded() && map->bounds().valid) {
        const nav::Aabb& box = map->bounds();
        area.centerX = (box.min.x + box.max.x) * 0.5f;
        area.centerY = (box.min.y + box.max.y) * 0.5f;
        const float halfX = (box.max.x - box.min.x) * 0.5f;
        const float halfY = (box.max.y - box.min.y) * 0.5f;
        area.halfExtent = std::min(halfX, halfY) * kWildAreaFraction;
        return area;
    }
    area.centerX = kSpawnX;
    area.centerY = kSpawnY;
    area.halfExtent = kDefaultWildHalfExtent;
    return area;
}

// 벽 안에 뜬 야생은 영원히 얼어붙는다. blockedAlong 이 출발점부터 훑으므로
// 어느 방향으로 가려 해도 첫 샘플에서 막힌다. 자리를 몇 번 다시 굴려 본다.
constexpr int kSpawnAttempts = 16;

}  // namespace

RoomManager::RoomManager(RoomSettings settings, std::map<std::uint32_t, InstanceType> types)
    : settings_(std::move(settings)), types_(std::move(types)) {
    if (settings_.capacity < 1) {
        settings_.capacity = 1;
    }

    // 회수를 0초로 두면 마지막 사람이 나간 그 순간 방이 사라진다. 세션 정리가
    // 프레임 처리와 겹칠 수 있어서(InstanceHandler::room_ 주석 참고) 그 사이에
    // 방이 없어지면 처리 중인 프레임이 사라진 방을 짚는다. 최소 유예를 둔다.
    if (settings_.emptyLinger < std::chrono::seconds{1}) {
        settings_.emptyLinger = std::chrono::seconds{1};
    }
}

RoomManager::~RoomManager() = default;

bool RoomManager::isKnownType(std::uint32_t type) const {
    return types_.count(type) != 0;
}

Room* RoomManager::createRoomLocked(std::uint32_t type) {
    // isKnownType 을 통과한 뒤에만 불린다.
    const InstanceType& config = types_.at(type);
    const Map* map = config.map;

    auto room = std::make_unique<Room>();
    room->id = nextRoomId_++;
    room->type = type;
    room->world.setMap(map);

    // 스폰 좌표(C++)와 배회 경로(Lua)는 난수원이 따로다. 둘 다 심어야 재현된다.
    // 방 번호를 섞어서 같은 씨앗으로 띄워도 방마다 배치가 다르게 나온다.
    const unsigned seed = settings_.wildSeed != 0 ? settings_.wildSeed + room->id : 0;

    const WildArea area = wildAreaFor(map);

    if (settings_.wildPerRoom > 0) {
        room->ai = std::make_unique<WildAi>();
        room->ai->seed(seed);
        room->ai->setArea(area);
        room->ai->setMap(map);
    }

    if (room->ai != nullptr) {
        std::mt19937 rng(seed != 0 ? seed : std::random_device{}());
        std::uniform_real_distribution<float> coordX(area.centerX - area.halfExtent,
                                                     area.centerX + area.halfExtent);
        std::uniform_real_distribution<float> coordY(area.centerY - area.halfExtent,
                                                     area.centerY + area.halfExtent);
        // 맵이 종족을 정해 뒀으면 그 안에서만 고른다 (main 이 읽을 때 보스를
        // 이미 거절한다). 안 정해 뒀으면 표 전체에서 보스만 뺀 것을 쓴다 —
        // 한쪽만 막으면 나머지로 새어 나온다.
        std::vector<std::uint16_t> pool = config.wildSpecies;
        if (pool.empty()) {
            for (const proto::SpeciesBase& base : proto::kSpecies) {
                if (proto::isWildSpawnable(base.dex)) {
                    pool.push_back(base.id);
                }
            }
        }
        std::uniform_int_distribution<std::size_t> fromPool(
            0, pool.empty() ? 0 : pool.size() - 1);
        const auto pickSpecies = [&](std::mt19937& gen) -> std::uint16_t {
            return pool.empty() ? std::uint16_t{0} : pool[fromPool(gen)];
        };

        // 지형이 있으면 navmesh 위에 설 수 있는 자리만 쓴다.
        const auto unusable = [map](float x, float y) {
            if (map == nullptr || !map->loaded()) {
                return false;
            }
            return !map->canStandAt(x, y, map->agent(), nullptr);
        };

        int spawned = 0;
        for (int i = 0; i < settings_.wildPerRoom; ++i) {
            float x = coordX(rng);
            float y = coordY(rng);
            for (int attempt = 0; attempt < kSpawnAttempts && unusable(x, y); ++attempt) {
                x = coordX(rng);
                y = coordY(rng);
            }
            if (unusable(x, y)) {
                continue;
            }
            const std::uint16_t species = pickSpecies(rng);
            if (species == 0) {
                break;  // 뽑을 종족이 없다
            }
            room->world.enterWild(kWildIdBase + static_cast<std::uint64_t>(i), species,
                                  data::Position{type, x, y, 0.f});
            ++spawned;
        }
        if (spawned < settings_.wildPerRoom) {
            spdlog::warn("room {}: {} wild pokemon skipped, no clear spawn point", room->id,
                         settings_.wildPerRoom - spawned);
        }
    }

    Room* raw = room.get();
    rooms_.push_back(std::move(room));
    spdlog::info("room {} opened (type {}, {} rooms total)", raw->id, type, rooms_.size());
    return raw;
}

Room* RoomManager::join(std::uint32_t type) {
    if (!isKnownType(type)) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 자리가 있는 첫 방. 앞에서부터 채워야 방이 흩어지지 않고, 빈 방이 생겨
    // 회수될 기회도 온다.
    int roomsOfType = 0;
    for (const auto& room : rooms_) {
        if (room->type != type) {
            continue;
        }
        ++roomsOfType;
        if (room->players < settings_.capacity) {
            ++room->players;
            return room.get();
        }
    }

    if (settings_.maxRoomsPerType > 0 && roomsOfType >= settings_.maxRoomsPerType) {
        return nullptr;
    }

    Room* room = createRoomLocked(type);
    room->players = 1;
    return room;
}

void RoomManager::leave(Room* room) {
    if (room == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (room->players > 0) {
        --room->players;
    }
    if (room->players == 0) {
        room->emptySince = std::chrono::steady_clock::now();
    }
}

void RoomManager::tickShard(unsigned shard, unsigned shardCount, float dt) {
    // 회수가 끼어들어 방을 지우지 못하게 공유 잠금으로 막는다. 틱끼리는
    // 서로 다른 방을 맡으므로 동시에 돌아도 된다.
    std::shared_lock<std::shared_mutex> alive(lifetimeMutex_);

    // 목록만 사본으로 뜨고 mutex_ 는 바로 놓는다. 틱이 오래 걸리는데 그동안
    // 잡고 있으면 다른 스레드의 join/leave 가 전부 밀린다.
    std::vector<Room*> mine;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mine.reserve(rooms_.size() / (shardCount == 0 ? 1 : shardCount) + 1);
        for (const auto& room : rooms_) {
            if (shardCount <= 1 || room->id % shardCount == shard) {
                mine.push_back(room.get());
            }
        }
    }

    for (Room* room : mine) {
        // 방마다 자기 Lua VM 이고, 한 방은 언제나 이 샤드가 맡는다.
        if (room->ai != nullptr) {
            room->world.advanceWild(dt, *room->ai);
        }
        room->world.tick();
    }
}

void RoomManager::reapEmpty() {
    // 배타 잠금. 어떤 샤드도 방을 돌리고 있지 않을 때만 지운다.
    std::unique_lock<std::shared_mutex> alive(lifetimeMutex_);
    std::lock_guard<std::mutex> lock(mutex_);

    const auto now = std::chrono::steady_clock::now();
    const auto expired = [&](const std::unique_ptr<Room>& room) {
        if (room->players > 0) {
            return false;
        }
        if (now - room->emptySince < settings_.emptyLinger) {
            return false;
        }
        spdlog::info("room {} closed (type {}, empty for {}s)", room->id, room->type,
                     settings_.emptyLinger.count());
        return true;
    };

    rooms_.erase(std::remove_if(rooms_.begin(), rooms_.end(), expired), rooms_.end());
}

std::size_t RoomManager::roomCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rooms_.size();
}

std::size_t RoomManager::playerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t total = 0;
    for (const auto& room : rooms_) {
        total += static_cast<std::size_t>(room->players);
    }
    return total;
}

std::string RoomManager::describe() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // 종류별로 묶는다. map 이라 출력 순서가 종류 번호 순으로 고정된다.
    std::map<std::uint32_t, std::pair<int, int>> byType;  // type -> (방 수, 인원)
    for (const auto& room : rooms_) {
        auto& entry = byType[room->type];
        ++entry.first;
        entry.second += room->players;
    }

    std::string out;
    for (const auto& [type, entry] : byType) {
        if (!out.empty()) {
            out += ", ";
        }
        out += "type=" + std::to_string(type) + " rooms=" + std::to_string(entry.first) +
               " players=" + std::to_string(entry.second);
    }
    return out.empty() ? "no rooms" : out;
}

}  // namespace heaven::instance
