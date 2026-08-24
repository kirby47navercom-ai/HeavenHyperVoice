#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "AuthTicket.h"
#include "DevStore.h"
#include "LoginHandler.h"
#include "OdbcStore.h"
#include "PasswordHash.h"
#include "ServerMain.h"

namespace {

struct Options {
    std::uint16_t port = 9100;
    std::string certFile = "certs/server.crt";
    std::string keyFile = "certs/server.key";
    std::string authKeyFile = "certs/auth.key";
    std::string keyId = "dev-1";
    std::string issuer = "hhv-login";
    std::string chatHost = "127.0.0.1";
    std::uint16_t chatPort = 9000;
    std::string fieldHost = "127.0.0.1";
    std::uint16_t fieldPort = 9200;
    std::int64_t ticketTtl = 60;
    unsigned threads = 0;
    unsigned authThreads = 4;
    bool verbose = false;

    // dev = 아무나 통과(개발용), odbc = 실제 DB 조회
    std::string accountStore = "odbc";

    // 로그인 후 캐릭터를 고를 때까지 연결을 열어두는 상한(초).
    int sessionLifetime = 120;
    heaven::data::OdbcSettings db;

    // 아래 셋 중 하나라도 지정되면 그 일만 하고 종료한다. 서버는 뜨지 않는다.
    bool hashOnly = false;
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
                 "  --field-host <h>   field host handed to clients (default 127.0.0.1)\n"
                 "  --field-port <n>   field port handed to clients (default 9200)\n"
                 "  --chat-host <h>    chat host handed to clients (default 127.0.0.1)\n"
                 "  --chat-port <n>    chat port handed to clients (default 9000)\n"
                 "  --ticket-ttl <s>   ticket lifetime in seconds (default 60)\n"
                 "  --threads <n>      IOCP worker threads (default: hardware concurrency)\n"
                 "  --auth-threads <n> threads for DB lookup and password verification\n"
                 "                     (default 4; these must never run on IOCP workers)\n"
                 "  --select-timeout <s> how long a client may stay connected while\n"
                 "                     choosing a character (default 120, 0 disables)\n"
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
                 "  --hash-password       prompt for a password and print its argon2id hash.\n"
                 "                        Use it to seed accounts.password_hash by hand. The\n"
                 "                        hash goes to stdout, timings to stderr. The password\n"
                 "                        is read from the console, never from the command line.\n";
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
        } else if (arg == "--field-host") {
            options.fieldHost = next("--field-host");
        } else if (arg == "--field-port") {
            options.fieldPort = static_cast<std::uint16_t>(std::stoi(next("--field-port")));
        } else if (arg == "--chat-host") {
            options.chatHost = next("--chat-host");
        } else if (arg == "--chat-port") {
            options.chatPort = static_cast<std::uint16_t>(std::stoi(next("--chat-port")));
        } else if (arg == "--ticket-ttl") {
            options.ticketTtl = std::stoll(next("--ticket-ttl"));
        } else if (arg == "--threads") {
            options.threads = static_cast<unsigned>(std::stoi(next("--threads")));
        } else if (arg == "--select-timeout") {
            options.sessionLifetime = std::stoi(next("--select-timeout"));
        } else if (arg == "--auth-threads") {
            options.authThreads = static_cast<unsigned>(std::stoi(next("--auth-threads")));
        } else if (arg == "--account-store") {
            options.accountStore = next("--account-store");
            if (options.accountStore != "dev" && options.accountStore != "odbc") {
                throw std::runtime_error("--account-store must be 'dev' or 'odbc'");
            }
        } else if (heaven::data::parseOdbcOption(arg, next, options.db)) {
            // --db-driver/host/port/name/user/conn. FieldServer 와 같은 표를 쓴다.
        } else if (arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "--hash-password") {
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
// `LoginServer --hash-password 2>NUL` 로 해시만 뽑아 쓸 수 있게 한다.
//
// 비밀번호는 콘솔에서 받는다. 인자로 받으면 프로세스 목록과 셸 히스토리에 남는다.
int printPasswordHash() {
    std::cerr << "password: " << std::flush;
    const std::string password = heaven::net::readHiddenLine();
    if (password.empty()) {
        std::cerr << "nothing entered" << std::endl;
        return 1;
    }

    const auto started = std::chrono::steady_clock::now();
    const std::string encoded = heaven::data::hashPassword(password);
    const auto hashed = std::chrono::steady_clock::now();

    // 만든 해시가 실제로 검증되는지 왕복 확인한다.
    const bool roundTrip = heaven::data::verifyPassword(password, encoded);
    const auto verified = std::chrono::steady_clock::now();
    const bool rejectsWrong = !heaven::data::verifyPassword(password + "x", encoded);

    const auto ms = [](auto from, auto to) {
        return std::chrono::duration<double, std::milli>(to - from).count();
    };

    // 인코딩 문자열이 이미 m/t/p 를 담고 있으므로 따로 찍지 않는다.
    std::cout << encoded << std::endl;

    std::cerr << "hash      " << ms(started, hashed) << " ms\n"
              << "verify    " << ms(hashed, verified) << " ms\n"
              << "round trip " << (roundTrip ? "ok" : "FAILED")
              << ", wrong password " << (rejectsWrong ? "rejected" : "ACCEPTED") << std::endl;

    return (roundTrip && rejectsWrong) ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseArgs(argc, argv);
        heaven::net::initLogging(options.verbose);

        // 인증서도 키도 필요 없는 경로들이다. 서버를 띄우기 전에 처리한다.
        if (options.hashOnly) {
            return printPasswordHash();
        }
        if (options.saveDbPassword) {
            return heaven::net::storePasswordInteractive(heaven::net::kDbCredentialTarget,
                                                         "database");
        }
        if (options.forgetDbPassword) {
            return heaven::net::erasePasswordAndReport(heaven::net::kDbCredentialTarget,
                                                       "database");
        }

        const auto files = heaven::net::resolveServerFiles(
            options.certFile, options.keyFile, options.authKeyFile, "ticket signing key");
        const std::string& authKey = files.ticketKey;

        heaven::net::TlsContext tls(files.certificate, files.privateKey);
        heaven::proto::TicketSigner signer(authKey, options.keyId);

        // 계정과 캐릭터는 인터페이스가 나뉘어 있지만 구현은 하나다.
        // store 가 소유하고, 아래 두 포인터는 같은 객체를 가리킨다.
        std::unique_ptr<heaven::data::DevStore> devStore;
        std::unique_ptr<heaven::data::OdbcStore> odbcStore;
        heaven::data::AccountStore* accounts = nullptr;
        heaven::data::CharacterStore* characters = nullptr;

        if (options.accountStore == "dev") {
            devStore = std::make_unique<heaven::data::DevStore>();
            accounts = devStore.get();
            characters = devStore.get();
        } else {
            heaven::data::OdbcSettings db = options.db;
            db.poolSize = options.authThreads;

            if (const auto password = heaven::net::databasePassword()) {
                db.password = *password;
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
            odbcStore = std::make_unique<heaven::data::OdbcStore>(db);
            accounts = odbcStore.get();
            characters = odbcStore.get();
        }

        // 인증 스레드 풀. server.run() 이 끝난 뒤 명시적으로 정리한다.
        heaven::net::WorkQueue authQueue(options.authThreads);

        heaven::login::LoginContext context;
        context.accounts = accounts;
        context.characters = characters;
        context.signer = &signer;
        context.authQueue = &authQueue;
        // 캐릭터를 고르면 이 목록의 서비스마다 티켓이 한 장씩 나간다.
        // 음성 서버가 생기면 여기에 한 줄 추가하면 된다.
        context.targets = {
            {std::string(heaven::proto::kAudienceField), options.fieldHost, options.fieldPort},
            {std::string(heaven::proto::kAudienceChat), options.chatHost, options.chatPort},
        };
        context.ticketTtlSeconds = options.ticketTtl;
        context.issuer = options.issuer;

        heaven::net::TlsServerOptions serverOptions;
        serverOptions.port = options.port;
        serverOptions.workerThreads = options.threads;
        // 로그인은 2왕복이라 첫 프레임 이후로도 연결이 열려 있다. 캐릭터를
        // 고르지 않는 연결이 슬롯을 무한정 잡지 못하게 상한을 둔다.
        serverOptions.maxSessionLifetime = std::chrono::seconds(options.sessionLifetime);

        heaven::net::TlsServer server(serverOptions, tls, [&context](heaven::net::TlsSession&) {
            return std::make_unique<heaven::login::LoginHandler>(context);
        });

        heaven::net::installConsoleHandler(server);

        spdlog::info("LoginServer listening on port {} (TLS, IOCP)", options.port);
        spdlog::info("signing key: {} (key_id={})", authKey, options.keyId);
        for (const auto& target : context.targets) {
            spdlog::info("issues '{}' tickets for {}:{}, valid {}s", target.service, target.host,
                         target.port, options.ticketTtl);
        }
        spdlog::info("account store: {} ({} auth threads)", accounts->describe(),
                     authQueue.threadCount());
        spdlog::info("registration: {}",
                     accounts->supportsRegistration() ? "enabled" : "disabled");
        spdlog::info("character creation: {} (up to {} per account, {}s to choose)",
                     characters->supportsCreation() ? "enabled" : "disabled",
                     heaven::data::kMaxCharactersPerAccount, options.sessionLifetime);
        if (options.accountStore == "dev") {
            spdlog::warn("ANY non-empty username is accepted with no password check.");
        }

        server.run();

        // 큐를 먼저 비워야 대기 중인 인증이 응답을 보내고 끝난다.
        // accounts 보다 먼저 정리해야 작업이 죽은 저장소를 만지지 않는다.
        authQueue.stop();
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("fatal: {}", e.what());
        return 1;
    }
}
