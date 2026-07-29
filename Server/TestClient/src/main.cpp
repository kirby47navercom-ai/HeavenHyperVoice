// TestClient - 개발/검증용 클라이언트.
//
// 기본 흐름: LoginServer 에 로그인 -> 티켓 수령 -> 응답이 알려준 ChatServer 로 접속.
//
// 모드
//   대화형 (기본)   : stdin 으로 계속 채팅한다. /quit 또는 EOF 로 종료.
//   스크립트 (--say): 메시지를 보내고 --wait 초 뒤 종료. 자동 검증용.
//   부하 (--flood)  : 지연 없이 n개 전송.
//   정체 (--stall)  : 접속만 하고 수신하지 않는다. 서버 전송 큐 상한 시험용.
//
// 개발 인증서는 자체 서명이라 TLS 검증을 끄고 붙는다 (Net 의 클라이언트 컨텍스트).
// 실제 클라이언트는 반드시 검증을 켜야 한다.

#include <conio.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "ChatCodec.h"
#include "LoginCodec.h"
#include "TlsClient.h"

namespace {

using heaven::net::TlsClient;
using heaven::proto::Bytes;

struct Options {
    std::string loginHost = "127.0.0.1";
    std::uint16_t loginPort = 9100;

    // 주지 않으면 콘솔에서 직접 입력받는다. 빈 비밀번호도 유효한 입력이므로
    // 문자열이 비었는지가 아니라 플래그로 지정 여부를 따진다.
    std::string username;
    std::string password;
    bool passwordProvided = false;

    // 로그인 응답을 무시하고 여기로 붙는다 (지정된 경우).
    std::string chatHost;
    std::uint16_t chatPort = 0;

    std::vector<std::string> says;
    int waitSeconds = 3;
    int floodCount = 0;
    int stallSeconds = 0;
    bool loginOnly = false;
    bool dumpTicket = false;

    // 주어지면 로그인을 건너뛰고 이 티켓을 쓴다. 빈 값도 유효한 시험 대상이므로
    // 문자열이 비었는지가 아니라 플래그로 지정 여부를 따진다.
    bool ticketProvided = false;
    std::string ticketHex;

    bool scripted() const {
        return !says.empty() || floodCount > 0 || stallSeconds > 0 || loginOnly;
    }
    bool interactive() const { return !scripted(); }
};

void printUsage() {
    std::cout << "TestClient - development client for the login + chat servers\n"
                 "\n"
                 "  --login-host <h>  login server host (default 127.0.0.1)\n"
                 "  --login-port <n>  login server port (default 9100)\n"
                 "  --user <name>     username. Prompted for if omitted and stdin is a console.\n"
                 "  --password <p>    password. Prompted for (hidden) if omitted and stdin is\n"
                 "                    a console. The dev account store ignores it either way.\n"
                 "  --chat-host <h>   override the chat host the login server hands back\n"
                 "  --chat-port <n>   override the chat port the login server hands back\n"
                 "  --login-only      log in, print the result, and exit without chatting\n"
                 "  --dump-ticket     print the ticket as hex (for tamper/replay testing)\n"
                 "  --ticket-hex <h>  skip login and present this ticket instead.\n"
                 "                    Requires --chat-host/--chat-port.\n"
                 "  --say <text>      send this message, may be repeated (scripted mode)\n"
                 "  --wait <sec>      scripted mode: linger this long before exiting (default 3)\n"
                 "  --flood <n>       send n messages as fast as possible (load testing)\n"
                 "  --stall <sec>     connect, say hello, then never read for sec seconds\n"
                 "                    (exercises the server's send-queue limit)\n"
                 "  --help            show this message\n";
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

        if (arg == "--login-host") {
            options.loginHost = next("--login-host");
        } else if (arg == "--login-port") {
            options.loginPort = static_cast<std::uint16_t>(std::stoi(next("--login-port")));
        } else if (arg == "--user") {
            options.username = next("--user");
        } else if (arg == "--password") {
            options.password = next("--password");
            options.passwordProvided = true;
        } else if (arg == "--chat-host") {
            options.chatHost = next("--chat-host");
        } else if (arg == "--chat-port") {
            options.chatPort = static_cast<std::uint16_t>(std::stoi(next("--chat-port")));
        } else if (arg == "--login-only") {
            options.loginOnly = true;
        } else if (arg == "--dump-ticket") {
            options.dumpTicket = true;
        } else if (arg == "--ticket-hex") {
            options.ticketHex = next("--ticket-hex");
            options.ticketProvided = true;
        } else if (arg == "--say") {
            options.says.push_back(next("--say"));
        } else if (arg == "--wait") {
            options.waitSeconds = std::stoi(next("--wait"));
        } else if (arg == "--flood") {
            options.floodCount = std::stoi(next("--flood"));
        } else if (arg == "--stall") {
            options.stallSeconds = std::stoi(next("--stall"));
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + std::string(arg));
        }
    }
    return options;
}

// stdin 이 진짜 콘솔일 때만 프롬프트를 띄운다. 파이프로 넘겨받는 자동 검증
// 스크립트가 입력 대기에서 멈추지 않게 하기 위한 것.
bool stdinIsConsole() {
    DWORD mode = 0;
    return ::GetConsoleMode(::GetStdHandle(STD_INPUT_HANDLE), &mode) != 0;
}

// 화면에 남기지 않고 비밀번호를 받는다. 콘솔 전용이라 conio 를 쓴다.
std::string readHiddenLine() {
    std::string out;
    for (;;) {
        const int ch = ::_getch();
        if (ch == '\r' || ch == '\n') {
            break;
        }
        if (ch == 3) {  // Ctrl+C
            std::cout << '\n';
            std::exit(1);
        }
        if (ch == '\b' || ch == 127) {
            if (!out.empty()) {
                out.pop_back();
                std::cout << "\b \b" << std::flush;
            }
            continue;
        }
        if (ch == 0 || ch == 0xE0) {  // 방향키 등 확장 코드는 두 바이트다
            ::_getch();
            continue;
        }
        out.push_back(static_cast<char>(ch));
        std::cout << '*' << std::flush;
    }
    std::cout << std::endl;
    return out;
}

std::string promptLine(const char* label) {
    std::cout << label << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) {
        throw std::runtime_error("no input");
    }
    return line;
}

std::string toHex(const Bytes& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes) {
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 0x0F]);
    }
    return out;
}

Bytes fromHex(std::string_view hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("hex string has an odd length");
    }
    const auto value = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::runtime_error("invalid hex digit");
    };

    Bytes out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>((value(hex[i]) << 4) | value(hex[i + 1])));
    }
    return out;
}

struct LoginResult {
    bool ok = false;
    std::string message;
    Bytes ticket;
    std::string chatHost;
    std::uint16_t chatPort = 0;
    std::string nickname;
};

// LoginServer 에 붙어 티켓을 받아온다. 서버가 응답 후 연결을 끊는다.
LoginResult login(const Options& options) {
    TlsClient client;
    LoginResult result;
    std::atomic<bool> answered{false};

    client.onFrame([&](const Bytes& body) {
        const auto* envelope = heaven::proto::verifyLoginEnvelope(body);
        if (envelope == nullptr ||
            envelope->payload_type() != HeavenLogin::Payload::LoginResponse) {
            result.message = "malformed login response";
            answered.store(true);
            return;
        }

        const auto* response = envelope->payload_as_LoginResponse();
        result.ok = response->ok();
        if (response->message() != nullptr) {
            result.message = response->message()->str();
        }
        if (response->ticket() != nullptr) {
            result.ticket.assign(response->ticket()->begin(), response->ticket()->end());
        }
        if (response->chat_host() != nullptr) {
            result.chatHost = response->chat_host()->str();
        }
        result.chatPort = response->chat_port();
        if (response->nickname() != nullptr) {
            result.nickname = response->nickname()->str();
        }
        answered.store(true);
    });

    client.connect(options.loginHost, options.loginPort);
    client.send(heaven::proto::encodeLoginRequest(options.username, options.password));

    // 서버가 응답 후 끊으므로 연결이 끝날 때까지 기다리면 된다.
    client.wait();

    if (!answered.load()) {
        result.ok = false;
        if (result.message.empty()) {
            result.message = "login server closed the connection without responding";
        }
    }
    return result;
}

std::atomic<bool> g_chatClosed{false};

void printChatFrame(const Bytes& body) {
    const auto* envelope = heaven::proto::verifyEnvelope(body);
    if (envelope == nullptr) {
        std::cout << "!! malformed frame" << std::endl;
        return;
    }

    switch (envelope->payload_type()) {
        case HeavenChat::Payload::Notice:
            std::cout << "* " << envelope->payload_as_Notice()->text()->c_str() << std::endl;
            break;
        case HeavenChat::Payload::Chat: {
            const auto* chat = envelope->payload_as_Chat();
            std::cout << '[' << chat->nickname()->c_str() << "] " << chat->text()->c_str()
                      << std::endl;
            break;
        }
        default:
            std::cout << "!! unexpected payload from server" << std::endl;
            break;
    }
}

int runChat(const Options& options, const LoginResult& session) {
    const std::string host = options.chatHost.empty() ? session.chatHost : options.chatHost;
    const std::uint16_t port = options.chatPort != 0 ? options.chatPort : session.chatPort;

    TlsClient client;
    if (options.stallSeconds == 0) {
        client.onFrame(printChatFrame);
    }
    client.onClosed([] { g_chatClosed.store(true); });

    client.connect(host, port);
    std::cout << "connected to " << host << ':' << port << " as '" << session.nickname << '\''
              << std::endl;

    client.send(heaven::proto::encodeHello(session.ticket));

    if (options.stallSeconds > 0) {
        // Hello 를 보낸 뒤 수신을 멈춘다. 소켓 수신 버퍼가 차면 서버의 전송이
        // 완료되지 않아 서버 쪽 전송 큐가 쌓이기 시작한다.
        client.pauseReceiving();
        std::cout << "stalling for " << options.stallSeconds << "s without reading the socket"
                  << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(options.stallSeconds));
        client.close();
        client.wait();
        return 0;
    }

    if (options.floodCount > 0) {
        std::cout << "flooding " << options.floodCount << " messages" << std::endl;
        for (int i = 0; i < options.floodCount; ++i) {
            client.send(heaven::proto::encodeSay("flood #" + std::to_string(i)));
        }
    }

    if (options.interactive()) {
        std::cout << "type a message and press Enter. /quit to exit." << std::endl;

        std::string line;
        while (!g_chatClosed.load() && std::getline(std::cin, line)) {
            if (line == "/quit") {
                break;
            }
            if (line.empty()) {
                continue;
            }
            client.send(heaven::proto::encodeSay(line));
        }
    } else {
        for (const std::string& text : options.says) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            client.send(heaven::proto::encodeSay(text));
        }
        std::this_thread::sleep_for(std::chrono::seconds(options.waitSeconds));
    }

    client.close();
    client.wait();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        // 콘솔을 UTF-8 로 맞춰 한글 입출력이 프로토콜 인코딩과 일치하게 한다.
        ::SetConsoleCP(CP_UTF8);
        ::SetConsoleOutputCP(CP_UTF8);

        Options options = parseArgs(argc, argv);

        // 로그인을 건너뛰고 주어진 티켓을 그대로 제시한다 (위조/만료/재사용 시험용).
        if (options.ticketProvided) {
            if (options.chatHost.empty() || options.chatPort == 0) {
                throw std::runtime_error("--ticket-hex requires --chat-host and --chat-port");
            }
            LoginResult injected;
            injected.ok = true;
            injected.ticket = fromHex(options.ticketHex);
            injected.chatHost = options.chatHost;
            injected.chatPort = options.chatPort;
            injected.nickname = "(from ticket)";
            std::cout << "presenting a supplied ticket of " << injected.ticket.size() << " bytes"
                      << std::endl;
            return runChat(options, injected);
        }

        // 로그인 경로에서만 자격증명이 필요하다.
        if (options.username.empty()) {
            if (!stdinIsConsole()) {
                throw std::runtime_error(
                    "--user is required when stdin is not a console (piped or redirected)");
            }
            options.username = promptLine("id: ");
            if (options.username.empty()) {
                throw std::runtime_error("id must not be empty");
            }
        }
        if (!options.passwordProvided && stdinIsConsole()) {
            std::cout << "password: " << std::flush;
            options.password = readHiddenLine();
        }

        const LoginResult session = login(options);
        if (!session.ok) {
            std::cerr << "login failed: " << session.message << std::endl;
            return 1;
        }
        std::cout << "login ok as '" << session.nickname << "', ticket " << session.ticket.size()
                  << " bytes, chat at " << session.chatHost << ':' << session.chatPort
                  << std::endl;
        if (options.dumpTicket) {
            std::cout << "ticket-hex " << toHex(session.ticket) << std::endl;
        }

        if (options.loginOnly) {
            return 0;
        }
        return runChat(options, session);
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << std::endl;
        return 1;
    }
}
