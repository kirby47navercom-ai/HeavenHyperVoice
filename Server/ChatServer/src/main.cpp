#include <spdlog/spdlog.h>
#include <windows.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "ChatHandler.h"
#include "DevPaths.h"
#include "Room.h"
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
};

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
        heaven::chat::Room room;

        // 공개키만 갖는다. 티켓을 검증할 수는 있어도 발급할 수는 없다.
        heaven::proto::PublicKeyRing keys;
        keys.add(options.keyId, authPub);

        heaven::net::TlsServerOptions serverOptions;
        serverOptions.port = options.port;
        serverOptions.workerThreads = options.threads;

        heaven::net::TlsServer server(serverOptions, tls, [&](heaven::net::TlsSession&) {
            return std::make_unique<heaven::chat::ChatHandler>(room, keys);
        });

        g_server = &server;
        ::SetConsoleCtrlHandler(consoleHandler, TRUE);

        spdlog::info("ChatServer listening on port {} (TLS, IOCP)", options.port);
        spdlog::info("certificate: {}", cert);
        spdlog::info("ticket public key: {} (key_id={})", authPub, options.keyId);

        server.run();

        g_server = nullptr;
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("fatal: {}", e.what());
        return 1;
    }
}
