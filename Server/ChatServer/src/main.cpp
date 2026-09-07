#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "ChatHandler.h"
#include "Credentials.h"
#include "InstancePresence.h"
#include "PartyStore.h"
#include "RedisClient.h"
#include "Room.h"
#include "ServerMain.h"

namespace {

struct Options {
    std::uint16_t port = 9000;
    std::string certFile = "certs/server.crt";
    std::string keyFile = "certs/server.key";
    std::string authPubFile = "certs/auth.pub";
    std::string keyId = "dev-1";
    unsigned threads = 0;

    // 파티 상태를 두는 곳. 서버 셋이 공유해야 해서 프로세스 메모리로는 안 된다.
    std::string redisHost = "127.0.0.1";
    std::uint16_t redisPort = 6379;
    bool useRedis = true;

    bool verbose = false;
};

// 생존 신호를 갱신하는 주기. TTL(30초)보다 충분히 짧아야 한다.
//
// 프레임이 올 때 갱신하지 않는 이유는, 가만히 서 있는 사람도 파티에 남아 있어야
// 하기 때문이다. 아무것도 안 치면 프레임이 오지 않는다.
constexpr std::chrono::seconds kHeartbeatInterval{10};

void printUsage() {
    std::cout << "ChatServer - TLS chat server (IOCP)\n"
                 "\n"
                 "  --port <n>          listen port (default 9000)\n"
                 "  --cert <path>       PEM certificate chain (default certs/server.crt)\n"
                 "  --key <path>        PEM private key (default certs/server.key)\n"
                 "  --auth-pubkey <p>   Ed25519 public key used to verify login tickets\n"
                 "                      (default certs/auth.pub, see tools\\gen-auth-key.ps1)\n"
                 "  --key-id <id>       key identifier this public key answers to (default dev-1)\n"
                 "  --threads <n>       IOCP worker threads (default: hardware concurrency)\n"
                 "  --redis-host <h>    party store host (default 127.0.0.1)\n"
                 "  --redis-port <n>    party store port (default 6379)\n"
                 "  --no-redis          run without the party store; party commands are refused\n"
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
        } else if (arg == "--redis-host") {
            options.redisHost = next("--redis-host");
        } else if (arg == "--redis-port") {
            options.redisPort = static_cast<std::uint16_t>(std::stoi(next("--redis-port")));
        } else if (arg == "--no-redis") {
            options.useRedis = false;
        } else if (arg == "--threads") {
            options.threads = static_cast<unsigned>(std::stoi(next("--threads")));
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

        const auto files = heaven::net::resolveServerFiles(
            options.certFile, options.keyFile, options.authPubFile, "ticket public key");

        heaven::net::TlsContext tls(files.certificate, files.privateKey);
        heaven::chat::Room room;

        // 공개키만 갖는다. 티켓을 검증할 수는 있어도 발급할 수는 없다.
        heaven::proto::PublicKeyRing keys;
        keys.add(options.keyId, files.ticketKey);

        // 파티는 Redis 에 있다. 없으면 파티 명령만 거절하고 채팅은 그대로 돈다.
        std::unique_ptr<heaven::net::RedisClient> redis;
        std::unique_ptr<heaven::party::PartyStore> party;
        std::unique_ptr<heaven::instancechat::InstancePresence> presence;
        if (options.useRedis) {
            heaven::net::RedisSettings redisSettings;
            redisSettings.host = options.redisHost;
            redisSettings.port = options.redisPort;
            if (const auto stored =
                    heaven::net::readStoredPassword(heaven::net::kRedisCredentialTarget)) {
                redisSettings.password = *stored;
            }
            redis = std::make_unique<heaven::net::RedisClient>(redisSettings);
            if (!redis->connect()) {
                spdlog::warn("party store unavailable at {}: {}", redis->target(),
                             redis->lastError());
                spdlog::warn("party commands will be refused");
                redis.reset();
            } else {
                party = std::make_unique<heaven::party::PartyStore>(*redis);
                presence = std::make_unique<heaven::instancechat::InstancePresence>(*redis);
            }
        }

        heaven::net::TlsServerOptions serverOptions;
        serverOptions.port = options.port;
        serverOptions.workerThreads = options.threads;

        heaven::net::TlsServer server(serverOptions, tls, [&](heaven::net::TlsSession&) {
            return std::make_unique<heaven::chat::ChatHandler>(room, keys, party.get(),
                                                               presence.get());
        });

        heaven::net::installConsoleHandler(server);

        // 접속 중인 사람의 파티 생존 신호를 계속 살려 둔다. 이게 없으면 가만히
        // 서 있는 사람이 30초 뒤 파티에서 빠진다.
        std::atomic<bool> heartbeatRunning{party != nullptr};
        std::thread heartbeat;
        if (party != nullptr) {
            heartbeat = std::thread([&] {
                while (heartbeatRunning.load(std::memory_order_acquire)) {
                    for (const std::uint64_t accountId : room.onlineAccounts()) {
                        party->touch(accountId);
                    }
                    // 종료 요청을 빨리 알아채도록 잘게 나눠 잔다.
                    for (int i = 0; i < 10 && heartbeatRunning.load(std::memory_order_acquire);
                         ++i) {
                        std::this_thread::sleep_for(kHeartbeatInterval / 10);
                    }
                }
            });
        }

        spdlog::info("ChatServer listening on port {} (TLS, IOCP)", options.port);
        spdlog::info("certificate: {}", files.certificate);
        spdlog::info("ticket public key: {} (key_id={})", files.ticketKey, options.keyId);
        if (party != nullptr) {
            spdlog::info("party store: {} (up to {} members, {}s grace on disconnect)",
                         redis->target(), heaven::party::kMaxMembers,
                         heaven::party::kMemberTtlSeconds);
        } else {
            spdlog::warn("party store: disabled");
        }

        server.run();

        heartbeatRunning.store(false, std::memory_order_release);
        if (heartbeat.joinable()) {
            heartbeat.join();
        }
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("fatal: {}", e.what());
        return 1;
    }
}
