#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include "FieldHandler.h"
#include "MapCollision.h"
#include "OdbcStore.h"
#include "PokemonSpecies.h"
#include "RedisClient.h"
#include "ServerMain.h"
#include "WildAi.h"
#include "World.h"

namespace {

struct Options {
    std::uint16_t port = 9200;
    std::string certFile = "certs/server.crt";
    std::string keyFile = "certs/server.key";
    std::string authPubFile = "certs/auth.pub";
    std::string keyId = "dev-1";
    unsigned threads = 0;
    bool verbose = false;

    // 입장/퇴장에서만 DB 를 쓴다. 로그인 서버만큼 필요하지 않다.
    unsigned dbThreads = 2;

    // 벽 충돌 맵. 없으면 검사하지 않는다.
    std::string mapFile;

    // 야생 포켓몬. count 가 0 이면 스폰하지 않는다.
    int wildCount = 12;
    std::string wildScript = "scripts/wild_ai.lua";
    unsigned wildSeed = 0;  // 0 이면 매 실행 다르게

    std::string redisHost = "127.0.0.1";
    std::uint16_t redisPort = 6379;
    bool useRedis = true;

    heaven::data::OdbcSettings db;

    // 지정되면 그 일만 하고 종료한다.
    bool saveRedisPassword = false;
    bool forgetRedisPassword = false;

    // 로그인 서버 없이 필드만 클라이언트와 붙여볼 때. 티켓 검증과 DB/Redis 를
    // 모두 건너뛴다. 되돌리려면 이 플래그만 빼면 된다.
    bool devNoAuth = false;
};

void printUsage() {
    std::cout << "FieldServer - TLS field server (IOCP)\n"
                 "\n"
                 "  --port <n>          listen port (default 9200)\n"
                 "  --cert <path>       PEM certificate chain (default certs/server.crt)\n"
                 "  --key <path>        PEM private key (default certs/server.key)\n"
                 "  --auth-pubkey <p>   Ed25519 public key used to verify tickets\n"
                 "  --key-id <id>       key identifier this public key answers to\n"
                 "  --threads <n>       IOCP worker threads (default: hardware concurrency)\n"
                 "  --db-threads <n>    threads for position load/save (default 2; entering\n"
                 "                      and leaving only, so fewer than the login server)\n"
                 "  --map <path>        wall collision map. Without it the server does\n"
                 "                      not check walls at all.\n"
                 "  --wild-count <n>    wild pokemon to spawn (default 12; 0 disables them\n"
                 "                      and the Lua script is not loaded at all)\n"
                 "  --wild-script <p>   Lua behaviour tree (default scripts/wild_ai.lua)\n"
                 "  --wild-seed <n>     fix spawn points and wander paths (default: random)\n"
                 "  --redis-host <h>    default 127.0.0.1\n"
                 "  --redis-port <n>    default 6379\n"
                 "  --no-redis          skip the position cache; load and save via the DB only\n"
                 "  --db-driver <name>  ODBC driver name (default: auto-detected)\n"
                 "  --db-host <h>       database host (default 127.0.0.1)\n"
                 "  --db-port <n>       database port (default 3306)\n"
                 "  --db-name <n>       database name (default hhv)\n"
                 "  --db-user <u>       database user (default hhv_server)\n"
                 "  --db-conn <str>     full ODBC connection string, overrides the above\n"
                 "  --verbose           enable debug logging\n"
                 "  --help              show this message\n"
                 "\n"
                 "Development only\n"
                 "  --dev-no-auth       accept Enter without a ticket and run with no\n"
                 "                      database or cache. The client sends its own name\n"
                 "                      and id in the Enter frame; nothing is persisted.\n"
                 "                      Brings the field up without the login server.\n"
                 "\n"
                 "Run once, then exit (no server is started)\n"
                 "  --save-redis-password    prompt for the Redis password and store it in the\n"
                 "                           Windows Credential Manager. The password never goes\n"
                 "                           on the command line.\n"
                 "  --forget-redis-password  remove the stored password.\n";
}

Options parseArgs(int argc, char** argv) {
    Options options;
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
        } else if (arg == "--map") {
            options.mapFile = next("--map");
        } else if (arg == "--wild-count") {
            options.wildCount = std::stoi(next("--wild-count"));
        } else if (arg == "--wild-script") {
            options.wildScript = next("--wild-script");
        } else if (arg == "--wild-seed") {
            options.wildSeed = static_cast<unsigned>(std::stoul(next("--wild-seed")));
        } else if (arg == "--redis-host") {
            options.redisHost = next("--redis-host");
        } else if (arg == "--redis-port") {
            options.redisPort = static_cast<std::uint16_t>(std::stoi(next("--redis-port")));
        } else if (arg == "--no-redis") {
            options.useRedis = false;
        } else if (arg == "--save-redis-password") {
            options.saveRedisPassword = true;
        } else if (arg == "--forget-redis-password") {
            options.forgetRedisPassword = true;
        } else if (heaven::data::parseOdbcOption(arg, next, options.db)) {
            // --db-driver/host/port/name/user/conn. LoginServer 와 같은 표를 쓴다.
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
        const Options options = parseArgs(argc, argv);
        heaven::net::initLogging(options.verbose);

        // 자격증명만 만지고 끝나는 경로들이다. 서버를 띄우기 전에 처리한다.
        if (options.forgetRedisPassword) {
            return heaven::net::erasePasswordAndReport(heaven::net::kRedisCredentialTarget,
                                                       "Redis");
        }
        if (options.saveRedisPassword) {
            return heaven::net::storePasswordInteractive(heaven::net::kRedisCredentialTarget,
                                                         "Redis");
        }

        const auto files = heaven::net::resolveServerFiles(
            options.certFile, options.keyFile, options.authPubFile, "ticket public key");
        const std::string& authPub = files.ticketKey;

        heaven::net::TlsContext tls(files.certificate, files.privateKey);

        // 공개키만 갖는다. 티켓을 검증할 수는 있어도 발급할 수는 없다.
        heaven::proto::PublicKeyRing keys;
        keys.add(options.keyId, authPub);

        // 개발 모드에서는 저장소를 아예 열지 않는다. MySQL 도 Memurai 도 없이 뜬다.
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

        // 위치 캐시. 없어도 DB 만으로 동작한다.
        std::unique_ptr<heaven::net::RedisClient> redis;
        if (options.useRedis && !options.devNoAuth) {
            heaven::net::RedisSettings redisSettings;
            redisSettings.host = options.redisHost;
            redisSettings.port = options.redisPort;
            if (const auto stored =
                    heaven::net::readStoredPassword(heaven::net::kRedisCredentialTarget)) {
                redisSettings.password = *stored;
            }
            redis = std::make_unique<heaven::net::RedisClient>(redisSettings);
            if (!redis->connect()) {
                spdlog::warn("position cache unavailable at {}: {}", redis->target(),
                             redis->lastError());
                spdlog::warn("positions will be read and written through the database only");
                redis.reset();
            }
        }

        heaven::net::WorkQueue dbQueue(options.dbThreads);
        heaven::field::World world;

        // 벽 충돌 맵. 경로를 줬는데 못 읽으면 기동을 멈춘다 — 검사가 켜진 줄
        // 알고 운영하는 것이 제일 나쁘다. 아예 안 주면 검사 없이 뜬다.
        heaven::field::MapCollision collision;
        if (!options.mapFile.empty()) {
            std::string mapError;
            if (!collision.loadFromFile(options.mapFile, mapError)) {
                throw std::runtime_error("map: " + mapError);
            }
            world.setCollision(&collision);
        }

        // 야생 포켓몬 AI. 스크립트를 못 읽으면 기동을 멈춘다 — 돌아다녀야 할
        // 야생이 조용히 얼어붙는 것보다 낫다. count 가 0 이면 아예 만들지 않는다.
        std::unique_ptr<heaven::field::WildAi> wildAi;
        if (options.wildCount > 0) {
            const std::string script =
                heaven::net::resolveResourcePath(options.wildScript, "wild AI script");
            wildAi = std::make_unique<heaven::field::WildAi>(script);

            // 스폰 좌표(C++)와 배회 경로(Lua)는 난수원이 따로다. 둘 다 심어야
            // --wild-seed 가 같은 판을 실제로 재현한다.
            wildAi->seed(options.wildSeed);
            std::mt19937 rng(options.wildSeed != 0 ? options.wildSeed : std::random_device{}());

            // 월드 전체에 흩뿌리지 않고 중앙 8000x8000 안에만 넣는다. 배회 구역도
            // 같은 상자다 (scripts/wild_ai.lua 의 AREA_*). 두 값이 어긋나면 스폰된
            // 자리에서 구역 안으로 걸어 들어가느라 처음 몇 초가 어색해진다.
            constexpr float kWildAreaHalfExtent = 4000.f;
            std::uniform_real_distribution<float> coord(
                heaven::proto::kSpawnX - kWildAreaHalfExtent,
                heaven::proto::kSpawnX + kWildAreaHalfExtent);
            std::uniform_int_distribution<int> species(1, static_cast<int>(
                                                              heaven::proto::kSpeciesCount));

            for (int i = 0; i < options.wildCount; ++i) {
                const heaven::data::Position start{0, coord(rng), coord(rng), 0.f};
                world.enterWild(heaven::field::kWildIdBase + static_cast<std::uint64_t>(i),
                                static_cast<std::uint16_t>(species(rng)), start);
            }
            spdlog::info("wild pokemon: {} spawned via {}", options.wildCount, script);
        }

        heaven::field::FieldContext context;
        context.world = &world;
        context.keys = &keys;
        context.characters = characters.get();
        context.devNoAuth = options.devNoAuth;
        context.dbQueue = &dbQueue;
        context.redis = redis.get();

        heaven::net::TlsServerOptions serverOptions;
        serverOptions.port = options.port;
        serverOptions.workerThreads = options.threads;

        heaven::net::TlsServer server(serverOptions, tls, [&context](heaven::net::TlsSession&) {
            return std::make_unique<heaven::field::FieldHandler>(context);
        });

        heaven::net::installConsoleHandler(server);

        spdlog::info("FieldServer listening on port {} (TLS, IOCP)", options.port);
        spdlog::info("ticket public key: {} (key_id={}, audience={})", authPub, options.keyId,
                     heaven::proto::kAudienceField);
        spdlog::info("world {:.0f}uu, {} sectors of {:.0f}uu, view {:.0f}/{:.0f}uu, {} Hz",
                     heaven::proto::kWorldSize, heaven::proto::kSectorCount,
                     heaven::proto::kSectorSize, heaven::proto::kEnterRadius,
                     heaven::proto::kExitRadius, heaven::proto::kTickHz);
        spdlog::info("characters: {} ({} db threads)",
                     characters ? characters->describe() : "disabled (--dev-no-auth)",
                     options.dbThreads);
        spdlog::info("position cache: {}", redis ? redis->target() : "disabled");
        if (collision.loaded()) {
            spdlog::info("wall collision: {} ({} walls)", options.mapFile, collision.wallCount());
        } else {
            spdlog::warn("wall collision: disabled (--map). Players can walk through anything.");
        }
        if (options.devNoAuth) {
            spdlog::warn("--dev-no-auth: ANY client may enter with a name of its choosing.");
            spdlog::warn("Tickets are not verified and nothing is saved.");
        }

        std::atomic<bool> running{true};

        // 20Hz 틱. 이번 주기에 움직인 것만 뷰어별로 묶어 내보낸다.
        std::thread ticker([&] {
            const auto period = std::chrono::milliseconds(1000 / heaven::proto::kTickHz);
            const float dt = 1.f / static_cast<float>(heaven::proto::kTickHz);
            while (running.load(std::memory_order_acquire)) {
                const auto deadline = std::chrono::steady_clock::now() + period;
                if (wildAi) {
                    world.advanceWild(dt, *wildAi);
                }
                world.tick();
                std::this_thread::sleep_until(deadline);
            }
        });

        // 60초마다 Redis 에 위치를 남긴다. 서버가 비정상 종료해도 이 시점까지는
        // 남는다. 캐시가 없으면 남길 곳이 없으니 스레드를 아예 띄우지 않는다.
        std::mutex wakeMutex;
        std::condition_variable wake;
        std::thread saver;
        if (redis != nullptr) {
            saver = std::thread([&] {
                while (running.load(std::memory_order_acquire)) {
                    {
                        std::unique_lock<std::mutex> lock(wakeMutex);
                        wake.wait_for(lock, std::chrono::seconds(60), [&] {
                            return !running.load(std::memory_order_acquire);
                        });
                    }
                    if (!running.load(std::memory_order_acquire)) {
                        return;
                    }
                    for (const auto& [characterId, position] : world.positions()) {
                        heaven::field::writeRedisPosition(*redis, characterId, position);
                    }
                }
            });
        }

        server.run();

        running.store(false, std::memory_order_release);
        wake.notify_all();
        ticker.join();
        if (saver.joinable()) {
            saver.join();
        }

        // TlsServer::run 은 워커를 먼저 정리한 뒤에 소켓을 닫는다. 그래서 종료
        // 시점에는 완료 통지가 더 오지 않고 FieldHandler::onClosed 도 불리지
        // 않는다 — 여기서 직접 저장하지 않으면 접속 중이던 전원의 위치가
        // 통째로 사라진다.
        if (characters != nullptr) {
            const auto remaining = world.positions();
            for (const auto& [characterId, position] : remaining) {
                characters->savePosition(characterId, position);
            }
            if (!remaining.empty()) {
                spdlog::info("saved {} position(s) on shutdown", remaining.size());
            }
        }

        // 큐를 먼저 비워야 대기 중인 위치 저장이 끝난다.
        dbQueue.stop();
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("fatal: {}", e.what());
        return 1;
    }
}
