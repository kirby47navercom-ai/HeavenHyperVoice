#include "RoomManager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <map>
#include <random>
#include <stdexcept>
#include <utility>

#include "InstanceGeometry.h"
#include "PokemonSpecies.h"
#include "WildBt.h"
#include "EarthScience/EnvironmentMovement.h"

namespace heaven::instance {

namespace {

// 정식 스폰 영역 데이터가 생기기 전까지는 인스턴스 시작 지점 주변에 뿌린다.
constexpr float kSpawnWildHalfExtent = 4000.f;

// 맵 경계에 딱 붙여 뿌리면 첫 배회에서 바로 지형 밖을 노리게 되므로 안쪽으로 민다.
constexpr float kWildAreaFraction = 0.8f;

// 야생을 뿌리고 배회시킬 구역. 플레이 테스트에서는 입장 직후 보여야 하므로
// 시작 지점 주변을 우선 쓴다. 맵이 붙어 있으면 실제로 설 수 있는지는 별도로 검증한다.
WildArea wildAreaFor(const Map *map) {
    WildArea area;
    area.centerX = kSpawnX;
    area.centerY = kSpawnY;
    area.halfExtent = kSpawnWildHalfExtent;

    if (map == nullptr || !map->loaded() || map->canStandAt(kSpawnX, kSpawnY, map->agent(), nullptr)) {
        return area;
    }

    // 시작 지점이 지형 밖인 맵에서는 최후 수단으로 맵 안쪽을 쓴다.
    if (map->bounds().valid) {
        const nav::Aabb &box = map->bounds();
        area.centerX = (box.min.x + box.max.x) * 0.5f;
        area.centerY = (box.min.y + box.max.y) * 0.5f;
        const float halfX = (box.max.x - box.min.x) * 0.5f;
        const float halfY = (box.max.y - box.min.y) * 0.5f;
        area.halfExtent = std::min(halfX, halfY) * kWildAreaFraction;
    }
    return area;
}

// 이동은 20Hz지만 날씨 패킷은 현실 1초마다 한 번이면 충분하다. 클라이언트가
// 사이 값을 보간하므로 네트워크와 서버 계산량이 방 수에 비례해 폭증하지 않는다.
constexpr double kWeatherBroadcastSeconds = 1.0;

} // namespace

RoomManager::RoomManager(RoomSettings settings, std::map<std::uint32_t, InstanceType> types)
    : settings_(std::move(settings)), types_(std::move(types)) {
    // 공용 세계 시간의 다섯 설정은 모든 종류에서 같아야 해요. 위도/지역 기후는 달라도 돼요.
    const auto sameClock=[](const InstanceWeatherProfile& a,const InstanceWeatherProfile& b) {
        return a.gameSecondsPerRealSecond==b.gameSecondsPerRealSecond && a.environment.daySeconds==b.environment.daySeconds &&
            a.environment.yearDays==b.environment.yearDays && a.environment.startHour==b.environment.startHour &&
            a.environment.startYearFraction==b.environment.startYearFraction;
    };
    for (const auto& [type, config] : types_) {
        if(!sameClock(types_.begin()->second.weather,config.weather))
            throw std::invalid_argument("Instance environment clocks must match across types");
        if (!config.map || !config.map->loaded()) {
            throw std::invalid_argument("Instance type " + std::to_string(type) + " requires shared collision");
        }
    }
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

Room *RoomManager::createRoomLocked(std::uint32_t type,std::uint32_t restoredId,const InstanceWeather* restored) {
    if(!restoredId && nextRoomId_==UINT32_MAX) throw std::runtime_error("Room IDs exhausted");
    // isKnownType 을 통과한 뒤에만 불린다.
    const InstanceType &config = types_.at(type);
    const Map *map = config.map;

    auto room = std::make_unique<Room>();
    room->id = restoredId ? restoredId : nextRoomId_++;
    room->type = type;
    room->world.setMap(map);
    room->weather.initialize(type, room->id, config.weather, worldClock_.elapsedRealSeconds());
    if(restored) room->weather=*restored;
    room->environment=room->weather.snapshot().environment;
    room->world.setEnvironment(movementEnvironment(config.weather.environment,room->weather.snapshot()));

    // 스폰 좌표와 배회 목표는 C++ 난수원을 쓴다. Lua BT 도 나중에 난수를
    // 쓸 수 있으므로 같은 seed 를 심어 둔다. 방 번호를 섞어서 같은 씨앗으로
    // 띄워도 방마다 배치가 다르게 나온다.
    const unsigned seed = settings_.wildSeed != 0 ? settings_.wildSeed + room->id : 0;

    const WildArea area = wildAreaFor(map);

    if (settings_.wildPerRoom > 0) {
        auto behavior = std::make_unique<WildBt>(settings_.wildAiScript);
        room->ai = std::make_unique<WildAi>(std::move(behavior));
        room->ai->seed(seed);
        room->ai->setArea(area);
        room->ai->setMap(map);
    }

    room->wildRandom.seed(seed ? seed : std::random_device{}());
    room->wildArea=area;
    room->wildRespawnRemaining.assign(static_cast<std::size_t>(std::max(0,settings_.wildPerRoom)),0);
    respawnWild(*room,0); // 최초 생성과 리스폰이 같은 환경 가중치/충돌 검사를 사용해요.

    Room *raw = room.get();
    rooms_.push_back(std::move(room));
    const InstanceWeatherSnapshot weather = raw->weather.snapshot();
    spdlog::info("room {} opened (type {}, {:.1f}C, {:.0f}% RH, {} rooms total)",
                 raw->id, type, weather.temperatureC, weather.relativeHumidityPct, rooms_.size());
    return raw;
}

Room *RoomManager::join(std::uint32_t type, std::uint32_t preferredRoomId) {
    if (!isKnownType(type)) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 파티가 지목한 방이 있으면 먼저 본다. 없어졌거나 꽉 찼으면 아래로 흘러간다.
    if (preferredRoomId != 0) {
        for (const auto &room : rooms_) {
            if (room->id == preferredRoomId && room->type == type && room->players < settings_.capacity) {
                room->waitingForRestoredEntry=false;
                ++room->players;
                return room.get();
            }
        }
    }

    // 자리가 있는 첫 방. 앞에서부터 채워야 방이 흩어지지 않고, 빈 방이 생겨
    // 회수될 기회도 온다.
    int roomsOfType = 0;
    for (const auto &room : rooms_) {
        if (room->type != type) {
            continue;
        }
        ++roomsOfType;
        if (room->players < settings_.capacity) {
            room->waitingForRestoredEntry=false;
            ++room->players;
            return room.get();
        }
    }

    if (settings_.maxRoomsPerType > 0 && roomsOfType >= settings_.maxRoomsPerType) {
        return nullptr;
    }

    Room *room = createRoomLocked(type);
    room->players = 1;
    return room;
}

void RoomManager::leave(Room *room) {
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
    struct TickRoom {
        Room *room = nullptr;
        bool occupied = false;
    };
    std::vector<TickRoom> mine;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mine.reserve(rooms_.size() / (shardCount == 0 ? 1 : shardCount) + 1);
        for (const auto &room : rooms_) {
            if (shardCount <= 1 || room->id % shardCount == shard) {
                mine.push_back({room.get(), room->players > 0});
            }
        }
    }

    for (const TickRoom target : mine) {
        Room *room = target.room;
        respawnWild(*room,dt);
        // 방마다 자기 Lua VM 이고, 한 방은 언제나 이 샤드가 맡는다.
        if (room->ai != nullptr) {
            room->ai->setVisibilityMultiplier(static_cast<float>(room->environment.visibilityMultiplier));
            room->world.advanceWild(dt, *room->ai, static_cast<float>(room->environment.movementMultiplier));
        }
        room->world.tick(dt);

        // 회수 유예 중에도 1초 간격 계산만 유지해요. 재입장 때 낮밤이 과거로 돌아가지 않아요.

        room->weatherBroadcastAccumulator += static_cast<double>(dt);
        if (room->weatherBroadcastAccumulator < kWeatherBroadcastSeconds) {
            continue;
        }

        const double elapsed = room->weatherBroadcastAccumulator;
        room->weatherBroadcastAccumulator = 0.0;
        room->weather.advance(elapsed);
        room->weather.synchronizeClock(worldClock_.elapsedRealSeconds());
        const InstanceWeatherSnapshot weather = room->weather.snapshot();
        room->environment=weather.environment;
        room->world.setEnvironment(movementEnvironment(types_.at(room->type).weather.environment,weather));
        if(target.occupied) room->world.broadcast(proto::encodeWeatherState(weather));
    }
}

void RoomManager::reapEmpty() {
    // 배타 잠금. 어떤 샤드도 방을 돌리고 있지 않을 때만 지운다.
    std::unique_lock<std::shared_mutex> alive(lifetimeMutex_);
    std::lock_guard<std::mutex> lock(mutex_);

    const auto now = std::chrono::steady_clock::now();
    const auto expired = [&](const std::unique_ptr<Room> &room) {
        if (room->players > 0 || room->waitingForRestoredEntry) {
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
    for (const auto &room : rooms_) {
        total += static_cast<std::size_t>(room->players);
    }
    return total;
}

std::string RoomManager::describe() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // 종류별로 묶는다. map 이라 출력 순서가 종류 번호 순으로 고정된다.
    std::map<std::uint32_t, std::pair<int, int>> byType; // type -> (방 수, 인원)
    for (const auto &room : rooms_) {
        auto &entry = byType[room->type];
        ++entry.first;
        entry.second += room->players;
    }

    std::string out;
    for (const auto &[type, entry] : byType) {
        if (!out.empty()) {
            out += ", ";
        }
        out += "type=" + std::to_string(type) + " rooms=" + std::to_string(entry.first) +
               " players=" + std::to_string(entry.second);
    }
    return out.empty() ? "no rooms" : out;
}

} // namespace heaven::instance
