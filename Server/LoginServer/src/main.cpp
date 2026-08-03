#include <spdlog/spdlog.h>
#include <windows.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "AuthTicket.h"
#include "Credentials.h"
#include "DevAccountStore.h"
#include "DevPaths.h"
#include "LoginHandler.h"
#include "OdbcAccountStore.h"
#include "PasswordHash.h"
#include "TlsServer.h"
#include "WorkQueue.h"

namespace {

heaven::net::TlsServer* g_server = nullptr;

// MSVC 는 getenv 를 안전하지 않다고 경고한다.
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
    unsigned authThreads = 4;
    bool verbose = false;

    // dev = 아무나 통과(개발용), odbc = 실제 DB 조회
    std::string accountStore = "odbc";
    heaven::login::OdbcSettings db;

    // 아래 셋 중 하나라도 지정되면 그 일만 하고 종료한다. 서버는 뜨지 않는다.
    bool hashOnly = false;
    std::string passwordToHash;
    bool saveDbPassword = false;
    bool forgetDbPassword = false;
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
                 "  --auth-threads <n> threads for DB lookup and password verification\n"
                 "                     (default 4; these must never run on IOCP workers)\n"
                 "  --verbose          enable debug logging\n"
                 "  --help             show this message\n"
                 "\n"
                 "Account storage\n"
                 "  --account-store <dev|odbc>  where accounts come from (default odbc).\n"
                 "                              dev accepts any username without a password.\n"
                 "  --db-driver <name>  ODBC driver name (default: auto-detected)\n"
                 "  --db-host <h>       database host (default 127.0.0.1)\n"
                 "  --db-port <n>       database port (default 3306)\n"
                 "  --db-name <n>       database name (default hhv)\n"
                 "  --db-user <u>       database user (default hhv_server)\n"
                 "  --db-conn <str>     full ODBC connection string, overrides the above\n"
                 "\n"
                 "Run once, then exit (no server is started)\n"
                 "  --save-db-password    prompt for the database password and store it in the\n"
                 "                        Windows Credential Manager. Do this once and the\n"
                 "                        launcher works by double-click, with no environment\n"
                 "                        setup. The password never goes on the command line.\n"
                 "  --forget-db-password  remove the stored password.\n"
                 "  --hash-password <p>   print an argon2id hash for <p>. Use it to seed\n"
                 "                        accounts.password_hash by hand. The hash goes to\n"
                 "                        stdout, timings to stderr.\n";
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
        } else if (arg == "--auth-threads") {
            options.authThreads = static_cast<unsigned>(std::stoi(next("--auth-threads")));
        } else if (arg == "--account-store") {
            options.accountStore = next("--account-store");
            if (options.accountStore != "dev" && options.accountStore != "odbc") {
                throw std::runtime_error("--account-store must be 'dev' or 'odbc'");
            }
        } else if (arg == "--db-driver") {
            options.db.driver = next("--db-driver");
        } else if (arg == "--db-host") {
            options.db.server = next("--db-host");
        } else if (arg == "--db-port") {
            options.db.port = static_cast<std::uint16_t>(std::stoi(next("--db-port")));
        } else if (arg == "--db-name") {
            options.db.database = next("--db-name");
        } else if (arg == "--db-user") {
            options.db.user = next("--db-user");
        } else if (arg == "--db-conn") {
            options.db.connectionString = next("--db-conn");
        } else if (arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "--hash-password") {
            options.passwordToHash = next("--hash-password");
            options.hashOnly = true;
        } else if (arg == "--save-db-password") {
            options.saveDbPassword = true;
        } else if (arg == "--forget-db-password") {
            options.forgetDbPassword = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + std::string(arg));
        }
    }
    return options;
}

// --hash-password 처리. 해시는 stdout 으로, 진단은 stderr 로 내보내
// `LoginServer --hash-password x 2>NUL` 로 해시만 뽑아 쓸 수 있게 한다.
int printPasswordHash(const std::string& password) {
    const auto params = heaven::login::currentHashParameters();

    const auto started = std::chrono::steady_clock::now();
    const std::string encoded = heaven::login::hashPassword(password);
    const auto hashed = std::chrono::steady_clock::now();

    // 만든 해시가 실제로 검증되는지 왕복 확인한다.
    const bool roundTrip = heaven::login::verifyPassword(password, encoded);
    const auto verified = std::chrono::steady_clock::now();
    const bool rejectsWrong = !heaven::login::verifyPassword(password + "x", encoded);

    const auto ms = [](auto from, auto to) {
        return std::chrono::duration<double, std::milli>(to - from).count();
    };

    std::cout << encoded << std::endl;

    std::cerr << "argon2id  m=" << params.memoryKiB << "KiB t=" << params.iterations
              << " p=" << params.parallelism << '\n'
              << "hash      " << ms(started, hashed) << " ms\n"
              << "verify    " << ms(hashed, verified) << " ms\n"
              << "round trip " << (roundTrip ? "ok" : "FAILED")
              << ", wrong password " << (rejectsWrong ? "rejected" : "ACCEPTED") << std::endl;

    return (roundTrip && rejectsWrong) ? 0 : 1;
}

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

int saveDatabasePassword() {
    std::cout << "database password: " << std::flush;
    const std::string password = readHiddenLine();
    if (password.empty()) {
        std::cerr << "nothing entered, not stored" << std::endl;
        return 1;
    }

    heaven::login::storePassword(heaven::login::kDbCredentialTarget, password);
    std::cout << "stored in the Windows Credential Manager as '"
              << heaven::login::kDbCredentialTarget << "'.\n"
              << "It is encrypted for your Windows account. The launcher can now start the\n"
              << "servers without HHV_DB_PASSWORD being set." << std::endl;
    return 0;
}

int forgetDatabasePassword() {
    const bool removed = heaven::login::erasePassword(heaven::login::kDbCredentialTarget);
    std::cout << (removed ? "stored database password removed"
                          : "no stored database password to remove")
              << std::endl;
    return 0;
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
        if (options.hashOnly) {
            return printPasswordHash(options.passwordToHash);
        }
        if (options.saveDbPassword) {
            return saveDatabasePassword();
        }
        if (options.forgetDbPassword) {
            return forgetDatabasePassword();
        }

        const std::string cert = heaven::net::resolveResourcePath(options.certFile, "certificate");
        const std::string key = heaven::net::resolveResourcePath(options.keyFile, "private key");
        const std::string authKey =
            heaven::net::resolveResourcePath(options.authKeyFile, "ticket signing key");

        heaven::net::TlsContext tls(cert, key);
        heaven::proto::TicketSigner signer(authKey, options.keyId);

        std::unique_ptr<heaven::login::AccountStore> accounts;
        if (options.accountStore == "dev") {
            accounts = std::make_unique<heaven::login::DevAccountStore>();
        } else {
            heaven::login::OdbcSettings db = options.db;
            db.poolSize = options.authThreads;

            // DB 비밀번호는 명령줄로 받지 않는다. 프로세스 목록에서 다 보이기 때문이다.
            // 환경변수를 먼저 본다 (CI 나 컨테이너용). 없으면 자격증명 관리자.
            if (const auto fromEnv = environmentValue("HHV_DB_PASSWORD")) {
                db.password = *fromEnv;
                spdlog::debug("database password came from HHV_DB_PASSWORD");
            } else if (const auto stored =
                           heaven::login::readStoredPassword(heaven::login::kDbCredentialTarget)) {
                db.password = *stored;
                spdlog::debug("database password came from the Windows Credential Manager");
            } else if (db.connectionString.empty()) {
                throw std::runtime_error(
                    "no database password available.\n"
                    "  Store it once (recommended, works with double-click):\n"
                    "    LoginServer.exe --save-db-password\n"
                    "  Or set it for this shell only:\n"
                    "    $env:HHV_DB_PASSWORD = '...'\n"
                    "  Or pass a full connection string with --db-conn,\n"
                    "  or run with --account-store dev to skip the database.");
            }
            accounts = std::make_unique<heaven::login::OdbcAccountStore>(db);
        }

        // 인증 스레드 풀. server.run() 이 끝난 뒤 명시적으로 정리한다.
        heaven::login::WorkQueue authQueue(options.authThreads);

        heaven::login::LoginContext context;
        context.accounts = accounts.get();
        context.signer = &signer;
        context.authQueue = &authQueue;
        context.chat = {options.chatHost, options.chatPort};
        context.ticketTtlSeconds = options.ticketTtl;
        context.issuer = options.issuer;
        context.audience = options.audience;

        heaven::net::TlsServerOptions serverOptions;
        serverOptions.port = options.port;
        serverOptions.workerThreads = options.threads;

        heaven::net::TlsServer server(serverOptions, tls, [&context](heaven::net::TlsSession&) {
            return std::make_unique<heaven::login::LoginHandler>(context);
        });

        g_server = &server;
        ::SetConsoleCtrlHandler(consoleHandler, TRUE);

        spdlog::info("LoginServer listening on port {} (TLS, IOCP)", options.port);
        spdlog::info("signing key: {} (key_id={})", authKey, options.keyId);
        spdlog::info("issues tickets for chat at {}:{}, audience={}, valid {}s", options.chatHost,
                     options.chatPort, options.audience, options.ticketTtl);
        spdlog::info("account store: {} ({} auth threads)", accounts->describe(),
                     authQueue.threadCount());
        if (options.accountStore == "dev") {
            spdlog::warn("ANY non-empty username is accepted with no password check.");
        }

        server.run();

        // 큐를 먼저 비워야 대기 중인 인증이 응답을 보내고 끝난다.
        // accounts 보다 먼저 정리해야 작업이 죽은 저장소를 만지지 않는다.
        authQueue.stop();

        g_server = nullptr;
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("fatal: {}", e.what());
        return 1;
    }
}
