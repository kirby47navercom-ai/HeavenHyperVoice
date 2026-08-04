#include <spdlog/spdlog.h>
#include <windows.h>

#include <atomic>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include "ChatHandler.h"
#include "Credentials.h"
#include "DevPaths.h"
#include "RedisClient.h"
#include "Room.h"
#include "SessionRegistry.h"
#include "TlsServer.h"

namespace {

heaven::net::TlsServer* g_server = nullptr;

struct Options {
    std::uint16_t port = 9000;
    std::string certFile = "certs/server.crt";
    std::string keyFile = "certs/server.key";
    std::string authPubFile = "certs/auth.pub";
    std::string keyId = "dev-1";
    unsigned threads = 0;
    bool verbose = false;

    // 세션 레지스트리(Redis/Memurai). 없어도 채팅은 계속된다.
    std::string redisHost = "127.0.0.1";
    std::uint16_t redisPort = 6379;
    bool useRedis = true;
    std::string serverId = "chat-1";

    // 지정되면 그 일만 하고 종료한다.
    bool saveRedisPassword = false;
    bool forgetRedisPassword = false;
};

// 자격증명 관리자에 보이는 이름.
constexpr const char* kRedisCredentialTarget = "HeavenHyperVoice/redis";

// 콘솔에서 에코 없이 한 줄 읽는다.
std::string readHiddenLine() {
    const HANDLE input = ::GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const bool isConsole = ::GetConsoleMode(input, &mode) != 0;
    if (isConsole) {
        ::SetConsoleMode(input, mode & ~static_cast<DWORD>(ENABLE_ECHO_INPUT));
    }
    std::string line;
    std::getline(std::cin, line);
    if (isConsole) {
        ::SetConsoleMode(input, mode);
        std::cout << std::endl;
    }
    return line;
}

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
        } else if (arg == "--redis-host") {
            options.redisHost = next("--redis-host");
        } else if (arg == "--redis-port") {
            options.redisPort = static_cast<std::uint16_t>(std::stoi(next("--redis-port")));
        } else if (arg == "--server-id") {
            options.serverId = next("--server-id");
        } else if (arg == "--no-redis") {
            options.useRedis = false;
        } else if (arg == "--save-redis-password") {
            options.saveRedisPassword = true;
        } else if (arg == "--forget-redis-password") {
            options.forgetRedisPassword = true;
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

        // 인증서도 키도 필요 없는 경로들이다. 서버를 띄우기 전에 처리한다.
        if (options.forgetRedisPassword) {
            const bool removed = heaven::net::erasePassword(kRedisCredentialTarget);
            std::cout << (removed ? "stored Redis password removed"
                                  : "no stored Redis password to remove")
                      << std::endl;
            return 0;
        }
        if (options.saveRedisPassword) {
            std::cout << "Redis password: " << std::flush;
            const std::string password = readHiddenLine();
            if (password.empty()) {
                std::cerr << "nothing entered, not stored" << std::endl;
                return 1;
            }
            heaven::net::storePassword(kRedisCredentialTarget, password);
            std::cout << "stored in the Windows Credential Manager as '"
                      << kRedisCredentialTarget << "'." << std::endl;
            return 0;
        }

        const std::string cert = heaven::net::resolveResourcePath(options.certFile, "certificate");
        const std::string key = heaven::net::resolveResourcePath(options.keyFile, "private key");
        const std::string authPub =
            heaven::net::resolveResourcePath(options.authPubFile, "ticket public key");

        heaven::net::TlsContext tls(cert, key);
        heaven::chat::Room room;

        // 공개키만 갖는다. 티켓을 검증할 수는 있어도 발급할 수는 없다.
        heaven::proto::PublicKeyRing keys;
        keys.add(options.keyId, authPub);

        // 세션 레지스트리는 없어도 되는 자원이다. 못 붙어도 채팅은 계속 받는다.
        std::unique_ptr<heaven::net::RedisClient> redis;
        std::unique_ptr<heaven::chat::SessionRegistry> registry;
        if (options.useRedis) {
            heaven::net::RedisSettings redisSettings;
            redisSettings.host = options.redisHost;
            redisSettings.port = options.redisPort;
            if (const auto stored = heaven::net::readStoredPassword(kRedisCredentialTarget)) {
                redisSettings.password = *stored;
            }

            redis = std::make_unique<heaven::net::RedisClient>(redisSettings);
            if (redis->connect()) {
                registry = std::make_unique<heaven::chat::SessionRegistry>(
                    *redis, options.serverId, heaven::chat::kSessionTtl);
            } else {
                spdlog::warn("session registry unavailable at {}: {}", redis->target(),
                             redis->lastError());
                spdlog::warn("chat still works; duplicate logins are only caught in-process");
                redis.reset();
            }
        }

        heaven::net::TlsServerOptions serverOptions;
        serverOptions.port = options.port;
        serverOptions.workerThreads = options.threads;

        heaven::net::TlsServer server(serverOptions, tls, [&](heaven::net::TlsSession&) {
            return std::make_unique<heaven::chat::ChatHandler>(room, keys, registry.get());
        });

        g_server = &server;
        ::SetConsoleCtrlHandler(consoleHandler, TRUE);

        spdlog::info("ChatServer listening on port {} (TLS, IOCP)", options.port);
        spdlog::info("certificate: {}", cert);
        spdlog::info("ticket public key: {} (key_id={})", authPub, options.keyId);
        if (registry) {
            spdlog::info("session registry: {} as '{}'", redis->target(), options.serverId);
        } else {
            spdlog::info("session registry: disabled");
        }

        // 등록의 TTL 을 주기적으로 늘린다. 이게 없으면 접속 중인데도 만료된다.
        std::atomic<bool> heartbeatRunning{registry != nullptr};
        std::thread heartbeat;
        if (registry) {
            heartbeat = std::thread([&] {
                while (heartbeatRunning.load(std::memory_order_acquire)) {
                    std::this_thread::sleep_for(heaven::chat::kSessionRefreshInterval);
                    if (!heartbeatRunning.load(std::memory_order_acquire)) {
                        break;
                    }
                    registry->refreshAll();
                }
            });
        }

        server.run();

        heartbeatRunning.store(false, std::memory_order_release);
        if (heartbeat.joinable()) {
            heartbeat.join();
        }

        g_server = nullptr;
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("fatal: {}", e.what());
        return 1;
    }
}
