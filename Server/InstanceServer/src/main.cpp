#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "InstanceGeometry.h"
#include "InstanceHandler.h"
#include "PokemonSpecies.h"
#include "MapCollision.h"
#include "OdbcStore.h"
#include "RoomManager.h"
#include "ServerMain.h"
#include "World.h"

namespace {

struct Options {
    std::uint16_t port = 9300;
    std::string certFile = "certs/server.crt";
    std::string keyFile = "certs/server.key";
    std::string authPubFile = "certs/auth.pub";
    std::string keyId = "dev-1";
    unsigned threads = 0;
    bool verbose = false;

    // 입장할 때만 DB 를 쓴다 (파트너와 파티). 위치는 저장하지 않는다.
    unsigned dbThreads = 2;

    // 방을 돌리는 스레드. 0 이면 하드웨어 동시성의 절반, 최소 1.
    unsigned tickThreads = 0;

    // 지형 파일 없이 받아줄 종류. 검사 없이 뜬다.
    std::string types;

    heaven::instance::RoomSettings rooms;

    // 종류마다 다른 지형. --instance-map <type>=<path> 로 하나씩 붙인다.
    std::map<std::uint32_t, std::string> maps;

    heaven::data::OdbcSettings db;

    bool devNoAuth = false;
};

void printUsage() {
    std::cout << "InstanceServer - TLS instance server (IOCP)\n"
                 "\n"
                 "같은 종류의 인스턴스를 정원만큼 채우고 넘치면 방을 하나 더 연다.\n"
                 "방끼리는 월드가 달라 서로 보이지 않는다.\n"
                 "\n"
                 "  --port <n>            listen port (default 9300)\n"
                 "  --cert <path>         PEM certificate chain (default certs/server.crt)\n"
                 "  --key <path>          PEM private key (default certs/server.key)\n"
                 "  --auth-pubkey <p>     Ed25519 public key used to verify tickets\n"
                 "  --key-id <id>         key identifier this public key answers to\n"
                 "  --threads <n>         IOCP worker threads (default: hardware concurrency)\n"
                 "  --db-threads <n>      threads for the character lookup on entry (default 2)\n"
                 "  --tick-threads <n>    threads that advance rooms (default: half the cores).\n"
                 "                        A room is always advanced by the same thread.\n"
                 "  --instance-map <t=p>  terrain for one instance type, repeatable.\n"
                 "                        e.g. --instance-map 1=maps/instance-1.txt\n"
                 "                        Listing a type here is enough to accept it.\n"
                 "  --instance-types <l>  extra types with no terrain, comma separated.\n"
                 "                        A type in neither option is refused - without\n"
                 "                        that a client could open unlimited rooms.\n"
                 "                        Default when both are empty: type 1, no terrain.\n"
                 "  --room-capacity <n>   players per room (default 20)\n"
                 "  --max-rooms <n>       rooms per type, 0 = unlimited (default 0)\n"
                 "  --room-idle <n>       seconds an empty room is kept before it closes\n"
                 "                        (default 60)\n"
                 "  --wild-per-room <n>   wild pokemon spawned when a room opens (default 12)\n"
                 "  --wild-script <p>     Lua actions (default scripts/wild_ai.lua)\n"
                 "  --wild-seed <n>       fix spawns and wander paths (default: random)\n"
                 "  --db-driver <name>    ODBC driver name (default: auto-detected)\n"
                 "  --db-host <h>         database host (default 127.0.0.1)\n"
                 "  --db-port <n>         database port (default 3306)\n"
                 "  --db-name <n>         database name (default hhv)\n"
                 "  --db-user <u>         database user (default hhv_server)\n"
                 "  --db-conn <str>       full ODBC connection string, overrides the above\n"
                 "  --verbose             enable debug logging\n"
                 "  --help                show this message\n"
                 "\n"
                 "Development only\n"
                 "  --dev-no-auth         accept Enter without a ticket and run with no\n"
                 "                        database. The client sends its own name and id.\n";
}

std::vector<std::uint32_t> parseTypes(const std::string& list) {
    std::vector<std::uint32_t> out;
    std::istringstream stream(list);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (!item.empty()) {
            out.push_back(static_cast<std::uint32_t>(std::stoul(item)));
        }
    }
    return out;
}

// 맵이 조용히 아무 일도 안 하는 흔한 두 경우를 기동 때 잡는다.
// 둘 다 문법은 멀쩡해서 로드는 성공하고, 그래서 알아채기 어렵다.
// 맵 파일에서 wild_species 줄만 읽는다. 도감번호로 적고 내부 번호로 돌려준다.
//
// 지형(MapCollision)이 같은 파일을 따로 읽는다. 한 줄 때문에 지형 파서에 종족
// 목록을 끼워 넣지 않았다 — 그쪽은 이름 그대로 충돌 판정만 안다.
//
// 표에 없는 도감번호는 기동에서 막는다. 조용히 넘기면 "적었는데 안 나온다" 가
// 되고, 그건 오타 하나를 찾느라 한참 헤매는 종류의 버그다.
std::vector<std::uint16_t> readWildSpecies(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open " + path);
    }

    std::vector<std::uint16_t> out;
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream stream(line);
        std::string type;
        stream >> type;
        if (type != "wild_species") {
            continue;
        }

        int dex = 0;
        while (stream >> dex) {
            const heaven::proto::SpeciesBase* species =
                heaven::proto::findSpeciesByDex(static_cast<std::uint16_t>(dex));
            if (species == nullptr) {
                throw std::runtime_error("unknown dex " + std::to_string(dex) + " in " + path +
                                         " line " + std::to_string(lineNumber));
            }
            // 보스는 레이드에서만 만나야 한다. 사냥터에 적어 두면 기동에서 막는다.
            if (!heaven::proto::isWildSpawnable(static_cast<std::uint16_t>(dex))) {
                throw std::runtime_error("dex " + std::to_string(dex) + " (" +
                                         std::string(species->name) +
                                         ") is a raid boss and cannot spawn in the wild: " +
                                         path + " line " + std::to_string(lineNumber));
            }
            out.push_back(species->id);
        }
        if (!stream.eof()) {
            throw std::runtime_error("malformed wild_species in " + path + " line " +
                                     std::to_string(lineNumber));
        }
    }
    return out;
}

void warnIfUseless(std::uint32_t type, const heaven::instance::MapCollision& collision) {
    // 스폰이 격자 밖이면 첫 표본부터 막혀서 그 방은 아무도 못 움직인다.
    if (collision.hasFloor()) {
        float floorZ = 0.f;
        if (!collision.floorAt(heaven::instance::kSpawnX, heaven::instance::kSpawnY, floorZ)) {
            spdlog::warn("instance type {}: heightmap does not cover the spawn point "
                         "({:.0f}, {:.0f}); nobody will be able to move",
                         type, heaven::instance::kSpawnX, heaven::instance::kSpawnY);
        }
    }

    // 구가 월드 정사각형을 통째로 삼키면 경계가 영영 안 걸린다.
    const heaven::instance::BoundsSphere& bounds = collision.bounds();
    if (!bounds.active()) {
        return;
    }
    float farthest = 0.f;
    for (const float cx : {0.f, heaven::instance::kWorldSize}) {
        for (const float cy : {0.f, heaven::instance::kWorldSize}) {
            const float dx = cx - bounds.center.x;
            const float dy = cy - bounds.center.y;
            farthest = std::max(farthest, std::sqrt(dx * dx + dy * dy));
        }
    }
    if (bounds.radius >= farthest) {
        spdlog::warn("instance type {}: bounds_sphere r={:.0f} at ({:.0f}, {:.0f}) already "
                     "contains the whole {:.0f}uu world (corner is {:.0f}uu away); "
                     "the boundary will never block anything",
                     type, bounds.radius, bounds.center.x, bounds.center.y,
                     heaven::instance::kWorldSize, farthest);
    }
}

Options parseArgs(int argc, char** argv) {
    Options options;
    options.rooms.wildScript = "scripts/wild_ai.lua";

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++i];
        };

        if (arg == "--port") {
            options.port = static_cast<std::uint16_t>(std::stoi(next("--port")));
        } else if (arg == "--cert") {
            options.certFile = next("--cert");
        } else if (arg == "--key") {
            options.keyFile = next("--key");
        } else if (arg == "--auth-pubkey") {
            options.authPubFile = next("--auth-pubkey");
        } else if (arg == "--key-id") {
            options.keyId = next("--key-id");
        } else if (arg == "--threads") {
            options.threads = static_cast<unsigned>(std::stoi(next("--threads")));
        } else if (arg == "--db-threads") {
            options.dbThreads = static_cast<unsigned>(std::stoi(next("--db-threads")));
        } else if (arg == "--tick-threads") {
            options.tickThreads = static_cast<unsigned>(std::stoi(next("--tick-threads")));
        } else if (arg == "--instance-types") {
            options.types = next("--instance-types");
        } else if (arg == "--room-capacity") {
            options.rooms.capacity = std::stoi(next("--room-capacity"));
        } else if (arg == "--max-rooms") {
            options.rooms.maxRoomsPerType = std::stoi(next("--max-rooms"));
        } else if (arg == "--room-idle") {
            options.rooms.emptyLinger = std::chrono::seconds(std::stoi(next("--room-idle")));
        } else if (arg == "--wild-per-room") {
            options.rooms.wildPerRoom = std::stoi(next("--wild-per-room"));
        } else if (arg == "--wild-script") {
            options.rooms.wildScript = next("--wild-script");
        } else if (arg == "--wild-seed") {
            options.rooms.wildSeed = static_cast<unsigned>(std::stoul(next("--wild-seed")));
        } else if (arg == "--instance-map") {
            const std::string pair = next("--instance-map");
            const std::size_t equals = pair.find('=');
            if (equals == 0 || equals == std::string::npos || equals + 1 >= pair.size()) {
                throw std::runtime_error("--instance-map wants <type>=<path>, got " + pair);
            }
            options.maps[static_cast<std::uint32_t>(std::stoul(pair.substr(0, equals)))] =
                pair.substr(equals + 1);
        } else if (heaven::data::parseOdbcOption(arg, next, options.db)) {
            // --db-driver/host/port/name/user/conn. 다른 서버와 같은 표를 쓴다.
        } else if (arg == "--dev-no-auth") {
            options.devNoAuth = true;
        } else if (arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + std::string(arg));
        }
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        Options options = parseArgs(argc, argv);
        heaven::net::initLogging(options.verbose);


        const auto files = heaven::net::resolveServerFiles(
            options.certFile, options.keyFile, options.authPubFile, "ticket public key");

        heaven::net::TlsContext tls(files.certificate, files.privateKey);

        // 공개키만 갖는다. 티켓을 검증할 수는 있어도 발급할 수는 없다.
        heaven::proto::PublicKeyRing keys;
        keys.add(options.keyId, files.ticketKey);

        // 스크립트를 여기서 한 번 읽어 본다. 방을 만들 때마다 실패하는 것보다
        // 기동에서 멈추는 편이 낫다 — 야생이 조용히 안 나오는 서버가 제일 나쁘다.
        if (options.rooms.wildPerRoom > 0) {
            options.rooms.wildScript =
                heaven::net::resolveResourcePath(options.rooms.wildScript, "wild AI script");
            heaven::instance::WildAi probe(options.rooms.wildScript);
            (void)probe;
        } else {
            options.rooms.wildScript.clear();
        }

        // 개발 모드에서는 저장소를 아예 열지 않는다.
        std::unique_ptr<heaven::data::OdbcStore> characters;
        if (!options.devNoAuth) {
            heaven::data::OdbcSettings db = options.db;
            db.poolSize = options.dbThreads;
            if (const auto password = heaven::net::databasePassword()) {
                db.password = *password;
            } else {
                throw std::runtime_error(
                    "no database password available.\n"
                    "  Store it once: LoginServer.exe --save-db-password\n"
                    "  Or set HHV_DB_PASSWORD for this shell.");
            }
            characters = std::make_unique<heaven::data::OdbcStore>(db);
        }

        // 종류별 지형. 경로를 줬는데 못 읽으면 기동을 멈춘다 — 검사가 켜진 줄
        // 알고 운영하는 것이 제일 나쁘다.
        //
        // MapCollision 은 복사가 안 되고 주소가 고정이어야 한다 (RoomManager 가
        // 포인터로 들고 방마다 넘긴다). unique_ptr 로 담는 이유다.
        std::map<std::uint32_t, std::unique_ptr<heaven::instance::MapCollision>> terrain;
        std::map<std::uint32_t, heaven::instance::InstanceType> types;

        for (const auto& [type, path] : options.maps) {
            auto collision = std::make_unique<heaven::instance::MapCollision>();
            std::string mapError;
            const std::string resolved =
                heaven::net::resolveResourcePath(path, "instance map");
            if (!collision->loadFromFile(resolved, mapError)) {
                throw std::runtime_error("instance " + std::to_string(type) + " map: " + mapError);
            }
            spdlog::info("instance type {}: {} ({})", type, resolved, collision->describe());
            warnIfUseless(type, *collision);

            types[type].collision = collision.get();
            types[type].wildSpecies = readWildSpecies(resolved);
            if (types[type].wildSpecies.empty()) {
                spdlog::warn("instance type {}: no wild_species line; every non-boss species "
                             "in the table can spawn", type);
            } else {
                std::string names;
                for (const std::uint16_t id : types[type].wildSpecies) {
                    if (!names.empty()) {
                        names += ", ";
                    }
                    names += std::string(heaven::proto::findSpecies(id)->name);
                }
                spdlog::info("instance type {}: wild species = {}", type, names);
            }
            terrain[type] = std::move(collision);
        }

        // 지형 없이 받아줄 종류. 이미 맵이 붙은 종류를 또 적으면 무시된다.
        for (const std::uint32_t type : parseTypes(options.types)) {
            if (types.count(type) == 0) {
                types[type];  // collision == nullptr, 지형 검사 없음
                spdlog::warn("instance type {}: no terrain, nothing blocks movement", type);
            }
        }

        // 둘 다 비었으면 1 번을 지형 없이 연다. 종류가 하나도 없으면 아무도
        // 못 들어오는 서버가 되는데, 그건 실수지 설정이 아니다.
        if (types.empty()) {
            types[1];
            spdlog::warn("no --instance-map or --instance-types; opening type 1 with no terrain");
        }

        heaven::net::WorkQueue dbQueue(options.dbThreads);
        heaven::instance::RoomManager rooms(options.rooms, types);

        heaven::instance::InstanceContext context;
        context.rooms = &rooms;
        context.keys = &keys;
        context.characters = characters.get();
        context.dbQueue = &dbQueue;
        context.devNoAuth = options.devNoAuth;

        heaven::net::TlsServerOptions serverOptions;
        serverOptions.port = options.port;
        serverOptions.workerThreads = options.threads;

        heaven::net::TlsServer server(serverOptions, tls, [&context](heaven::net::TlsSession&) {
            return std::make_unique<heaven::instance::InstanceHandler>(context);
        });

        heaven::net::installConsoleHandler(server);

        unsigned tickThreads = options.tickThreads;
        if (tickThreads == 0) {
            tickThreads = std::thread::hardware_concurrency() / 2;
        }
        if (tickThreads == 0) {
            tickThreads = 1;
        }

        spdlog::info("InstanceServer listening on port {} (TLS, IOCP)", options.port);
        spdlog::info("ticket public key: {} (key_id={}, audience={})", files.ticketKey,
                     options.keyId, heaven::proto::kAudienceInstance);
        {
            std::string list;
            for (const auto& [type, entry] : types) {
                if (!list.empty()) {
                    list += ", ";
                }
                list += std::to_string(type);
                list += entry.collision != nullptr ? " (terrain)" : " (no terrain)";
            }
            spdlog::info("instance types: {}", list);
        }
        spdlog::info("rooms: {} players each, {} per type, empty for {}s then closed",
                     options.rooms.capacity,
                     options.rooms.maxRoomsPerType == 0
                         ? std::string("unlimited")
                         : std::to_string(options.rooms.maxRoomsPerType),
                     options.rooms.emptyLinger.count());
        spdlog::info("wild pokemon: {} per room, {} tick thread(s)", options.rooms.wildPerRoom,
                     tickThreads);
        spdlog::info("characters: {} ({} db threads)",
                     characters ? characters->describe() : "disabled (--dev-no-auth)",
                     options.dbThreads);
        if (options.devNoAuth) {
            spdlog::warn("--dev-no-auth: ANY client may enter with a name of its choosing.");
        }

        std::atomic<bool> running{true};

        // 20Hz 틱. 스레드마다 자기 몫의 방만 돈다 (id % tickThreads).
        // 한 방은 언제나 같은 스레드가 맡으므로 그 방의 Lua VM 이 겹치지 않는다.
        std::vector<std::thread> tickers;
        tickers.reserve(tickThreads);
        for (unsigned shard = 0; shard < tickThreads; ++shard) {
            tickers.emplace_back([&, shard, tickThreads] {
                const auto period = std::chrono::milliseconds(1000 / heaven::instance::kTickHz);
                const float dt = 1.f / static_cast<float>(heaven::instance::kTickHz);
                float sinceReap = 0.f;

                while (running.load(std::memory_order_acquire)) {
                    const auto deadline = std::chrono::steady_clock::now() + period;
                    rooms.tickShard(shard, tickThreads, dt);

                    // 빈 방 회수는 한 스레드만 한다. 배타 잠금이라 다른 샤드가
                    // 잠깐 기다리지만, 1초에 한 번이고 방 목록만 훑는다.
                    if (shard == 0) {
                        sinceReap += dt;
                        if (sinceReap >= 1.f) {
                            sinceReap = 0.f;
                            rooms.reapEmpty();
                        }
                    }
                    std::this_thread::sleep_until(deadline);
                }
            });
        }

        server.run();

        running.store(false, std::memory_order_release);
        for (std::thread& ticker : tickers) {
            ticker.join();
        }

        // 인스턴스는 위치를 저장하지 않으므로 종료할 때 내보낼 것이 없다.
        // 큐만 비워 대기 중인 입장 처리가 끝나게 한다.
        dbQueue.stop();
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("fatal: {}", e.what());
        return 1;
    }
}
