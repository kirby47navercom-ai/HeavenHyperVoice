#pragma once

// 세 서버의 main 이 똑같이 하던 것들. 인자 파싱만 각자 다르다.

#include <spdlog/spdlog.h>
#include <windows.h>

#include <cstdlib>
#include <optional>
#include <string>

#include "Credentials.h"
#include "DevPaths.h"
#include "TlsServer.h"

namespace heaven::net {

// MSVC 는 getenv 를 안전하지 않다고 경고한다.
inline std::optional<std::string> environmentValue(const char* name) {
    char* buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) {
        return std::nullopt;
    }
    std::string value(buffer);
    std::free(buffer);
    return value;
}

// DB 비밀번호. 환경변수를 먼저 본다 (CI 나 컨테이너용). 없으면 자격증명 관리자.
// 명령줄로는 받지 않는다 — 프로세스 목록에서 다 보이기 때문이다.
inline std::optional<std::string> databasePassword() {
    if (auto fromEnv = environmentValue("HHV_DB_PASSWORD")) {
        spdlog::debug("database password came from HHV_DB_PASSWORD");
        return fromEnv;
    }
    if (auto stored = readStoredPassword(kDbCredentialTarget)) {
        spdlog::debug("database password came from the Windows Credential Manager");
        return stored;
    }
    return std::nullopt;
}

inline void initLogging(bool verbose) {
    spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
}

// Ctrl+C 로 서버를 멈춘다. 콘솔 핸들러가 전역을 요구하므로 여기 둔다.
inline TlsServer* g_server = nullptr;

inline BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        spdlog::info("shutting down");
        if (g_server != nullptr) {
            g_server->stop();
        }
        return TRUE;
    }
    return FALSE;
}

inline void installConsoleHandler(TlsServer& server) {
    g_server = &server;
    ::SetConsoleCtrlHandler(consoleHandler, TRUE);
}

// TLS 인증서와 티켓 키 경로. 세 서버가 같은 파일을 쓴다.
struct ServerFiles {
    std::string certificate;
    std::string privateKey;
    std::string ticketKey;  // 로그인은 개인키, 나머지는 공개키
};

inline ServerFiles resolveServerFiles(const std::string& cert, const std::string& key,
                                      const std::string& ticketKey, const char* ticketKeyLabel) {
    return {resolveResourcePath(cert, "certificate"), resolveResourcePath(key, "private key"),
            resolveResourcePath(ticketKey, ticketKeyLabel)};
}

}  // namespace heaven::net
