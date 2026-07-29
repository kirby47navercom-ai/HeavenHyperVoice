#include <spdlog/spdlog.h>
#include <windows.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "AuthTicket.h"
#include "DevAccountStore.h"
#include "DevPaths.h"
#include "LoginHandler.h"
#include "TlsServer.h"

namespace {

heaven::net::TlsServer* g_server = nullptr;

struct Options {
    std::uint16_t port = 9100;
    std::string certFile = "certs/server.crt";
    std::string keyFile = "certs/server.key";
    std::string authKeyFile = "certs/auth.key";
    std::string keyId = "dev-1";
    std::string issuer = "hhv-login";
    std::string audience = std::string(heaven::proto::kAudienceChat);
    std::string chatHost = "127.0.0.1";
    std::uint16_t chatPort = 9000;
    std::int64_t ticketTtl = 60;
    unsigned threads = 0;
    bool verbose = false;
};

void printUsage() {
    std::cout << "LoginServer - TLS login server (IOCP)\n"
                 "\n"
                 "  --port <n>         listen port (default 9100)\n"
                 "  --cert <path>      PEM certificate chain (default certs/server.crt)\n"
                 "  --key <path>       PEM private key (default certs/server.key)\n"
                 "  --auth-key <path>  Ed25519 private key for signing tickets\n"
                 "                     (default certs/auth.key, see tools\\gen-auth-key.ps1)\n"
                 "  --key-id <id>      key identifier embedded in tickets (default dev-1)\n"
                 "  --issuer <name>    issuer claim (default hhv-login)\n"
                 "  --audience <name>  audience claim: which service the ticket is for\n"
                 "                     (default chat; a verifier rejects any other value)\n"
                 "  --chat-host <h>    chat host handed to clients (default 127.0.0.1)\n"
                 "  --chat-port <n>    chat port handed to clients (default 9000)\n"
                 "  --ticket-ttl <s>   ticket lifetime in seconds (default 60)\n"
                 "  --threads <n>      IOCP worker threads (default: hardware concurrency)\n"
                 "  --verbose          enable debug logging\n"
                 "  --help             show this message\n";
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
        } else if (arg == "--auth-key") {
            options.authKeyFile = next("--auth-key");
        } else if (arg == "--key-id") {
            options.keyId = next("--key-id");
        } else if (arg == "--issuer") {
            options.issuer = next("--issuer");
        } else if (arg == "--audience") {
            options.audience = next("--audience");
        } else if (arg == "--chat-host") {
            options.chatHost = next("--chat-host");
        } else if (arg == "--chat-port") {
            options.chatPort = static_cast<std::uint16_t>(std::stoi(next("--chat-port")));
        } else if (arg == "--ticket-ttl") {
            options.ticketTtl = std::stoll(next("--ticket-ttl"));
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
        const std::string authKey =
            heaven::net::resolveResourcePath(options.authKeyFile, "ticket signing key");

        heaven::net::TlsContext tls(cert, key);
        heaven::proto::TicketSigner signer(authKey, options.keyId);
        heaven::login::DevAccountStore accounts;
        heaven::login::ChatEndpoint chat{options.chatHost, options.chatPort};

        heaven::net::TlsServerOptions serverOptions;
        serverOptions.port = options.port;
        serverOptions.workerThreads = options.threads;

        heaven::net::TlsServer server(serverOptions, tls, [&](heaven::net::TlsSession&) {
            return std::make_unique<heaven::login::LoginHandler>(
                accounts, signer, chat, options.ticketTtl, options.issuer, options.audience);
        });

        g_server = &server;
        ::SetConsoleCtrlHandler(consoleHandler, TRUE);

        spdlog::info("LoginServer listening on port {} (TLS, IOCP)", options.port);
        spdlog::info("signing key: {} (key_id={})", authKey, options.keyId);
        spdlog::info("issues tickets for chat at {}:{}, audience={}, valid {}s", options.chatHost,
                     options.chatPort, options.audience, options.ticketTtl);
        spdlog::warn("account store: {}", accounts.describe());
        spdlog::warn("ANY non-empty username is accepted. Replace AccountStore before shipping.");

        server.run();

        g_server = nullptr;
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("fatal: {}", e.what());
        return 1;
    }
}
