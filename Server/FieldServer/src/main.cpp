#include <spdlog/spdlog.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include "Credentials.h"
#include "DevPaths.h"
#include "FieldHandler.h"
#include "OdbcStore.h"
#include "RedisClient.h"
#include "TlsServer.h"
#include "WorkQueue.h"
#include "World.h"

namespace {

heaven::net::TlsServer* g_server = nullptr;

std::optional<std::string> environmentValue(const char* name) {
    char* buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) {
        return std::nullopt;
    }
    std::string value(buffer);
    std::free(buffer);
    return value;
}

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

    std::string redisHost = "127.0.0.1";
    std::uint16_t redisPort = 6379;
    bool useRedis = true;

    heaven::data::OdbcSettings db;
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
                 "  --redis-host <h>    default 127.0.0.1\n"
                 "  --redis-port <n>    default 6379\n"
                 "  --no-redis          skip the position cache; load and save via the DB only\n"
                 "  --db-host <h>       database host (default 127.0.0.1)\n"
                 "  --db-name <n>       database name (default hhv)\n"
                 "  --db-user <u>       database user (default hhv_server)\n"
                 "  --verbose           enable debug logging\n"
                 "  --help              show this message\n";
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
        } else if (arg == "--redis-host") {
            options.redisHost = next("--redis-host");
        } else if (arg == "--redis-port") {
            options.redisPort = static_cast<std::uint16_t>(std::stoi(next("--redis-port")));
        } else if (arg == "--no-redis") {
            options.useRedis = false;
        } else if (arg == "--db-host") {
            options.db.server = next("--db-host");
        } else if (arg == "--db-name") {
            options.db.database = next("--db-name");
        } else if (arg == "--db-user") {
            options.db.user = next("--db-user");
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

BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        spdlog::info("shutting down");
        if (g_server != nullptr) {
            g_server->stop();
        }
        return TRUE;
    }
    return FALSE;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseArgs(argc, argv);
        spdlog::set_level(options.verbose ? spdlog::level::debug : spdlog::level::info);
        spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

        const std::string cert = heaven::net::resolveResourcePath(options.certFile, "certificate");
        const std::string key = heaven::net::resolveResourcePath(options.keyFile, "private key");
        const std::string authPub =
            heaven::net::resolveResourcePath(options.authPubFile, "ticket public key");

        heaven::net::TlsContext tls(cert, key);

        // 공개키만 갖는다. 티켓을 검증할 수는 있어도 발급할 수는 없다.
        heaven::proto::PublicKeyRing keys;
        keys.add(options.keyId, authPub);

        heaven::data::OdbcSettings db = options.db;
        db.poolSize = options.dbThreads;
        if (const auto fromEnv = environmentValue("HHV_DB_PASSWORD")) {
            db.password = *fromEnv;
        } else if (const auto stored =
                       heaven::net::readStoredPassword(heaven::net::kDbCredentialTarget)) {
            db.password = *stored;
        } else {
            throw std::runtime_error(
                "no database password available.\n"
                "  Store it once: LoginServer.exe --save-db-password\n"
                "  Or set HHV_DB_PASSWORD for this shell.");
        }
        heaven::data::OdbcStore characters(db);

        // 위치 캐시. 없어도 DB 만으로 동작한다.
        std::unique_ptr<heaven::net::RedisClient> redis;
        if (options.useRedis) {
            heaven::net::RedisSettings redisSettings;
            redisSettings.host = options.redisHost;
            redisSettings.port = options.redisPort;
            if (const auto stored =
                    heaven::net::readStoredPassword("HeavenHyperVoice/redis")) {
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

        heaven::field::FieldContext context;
        context.world = &world;
        context.keys = &keys;
        context.characters = &characters;
        context.dbQueue = &dbQueue;
        context.redis = redis.get();

        heaven::net::TlsServerOptions serverOptions;
        serverOptions.port = options.port;
        serverOptions.workerThreads = options.threads;

        heaven::net::TlsServer server(serverOptions, tls, [&context](heaven::net::TlsSession&) {
            return std::make_unique<heaven::field::FieldHandler>(context);
        });

        g_server = &server;
        ::SetConsoleCtrlHandler(consoleHandler, TRUE);

        spdlog::info("FieldServer listening on port {} (TLS, IOCP)", options.port);
        spdlog::info("ticket public key: {} (key_id={}, audience={})", authPub, options.keyId,
                     heaven::proto::kAudienceField);
        spdlog::info("world {:.0f}uu, {} sectors of {:.0f}uu, view {:.0f}/{:.0f}uu, {} Hz",
                     heaven::proto::kWorldSize, heaven::proto::kSectorCount,
                     heaven::proto::kSectorSize, heaven::proto::kEnterRadius,
                     heaven::proto::kExitRadius, heaven::proto::kTickHz);
        spdlog::info("characters: {} ({} db threads)", characters.describe(), options.dbThreads);
        spdlog::info("position cache: {}", redis ? redis->target() : "disabled");

        std::atomic<bool> running{true};

        // 20Hz 틱. 이번 주기에 움직인 것만 뷰어별로 묶어 내보낸다.
        std::thread ticker([&] {
            const auto period = std::chrono::milliseconds(1000 / heaven::proto::kTickHz);
            while (running.load(std::memory_order_acquire)) {
                const auto deadline = std::chrono::steady_clock::now() + period;
                world.tick();
                std::this_thread::sleep_until(deadline);
            }
        });

        // 60초마다 Redis 에 위치를 남긴다. 서버가 비정상 종료해도 이 시점까지는 남는다.
        std::thread saver([&] {
            while (running.load(std::memory_order_acquire)) {
                for (int i = 0; i < 60 && running.load(std::memory_order_acquire); ++i) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (!running.load(std::memory_order_acquire) || redis == nullptr) {
                    continue;
                }
                for (const auto& [characterId, position] : world.positions()) {
                    heaven::field::writeRedisPosition(*redis, characterId, position);
                }
            }
        });

        server.run();

        running.store(false, std::memory_order_release);
        ticker.join();
        saver.join();

        // 큐를 먼저 비워야 대기 중인 위치 저장이 끝난다.
        dbQueue.stop();
        g_server = nullptr;
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("fatal: {}", e.what());
        return 1;
    }
}
