#pragma once

// 세 서버의 main 이 똑같이 하던 것들. 인자 파싱만 각자 다르다.

#include <spdlog/spdlog.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>

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

// tag 는 어느 서버가 찍은 줄인지다 (chat, field, instance, login). 런처가 넷을
// 한 콘솔에 모아 찍으므로, 레벨(info/warning)보다 이쪽이 훨씬 자주 필요하다.
// 레벨은 %^%$ 안에 넣은 덕에 색으로 남는다 — 경고는 노랑, 오류는 빨강이다.
inline void initLogging(bool verbose, const char* tag) {
    spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
    spdlog::set_pattern(std::string("[%H:%M:%S.%e] [%^") + tag + "%$] %v");
}

// 고정 주기 틱의 대기와 감시.
//
// sleep_until 을 쓰지 않는다. 윈도우의 기본 타이머 눈금은 15.6ms 라 50ms 를
// 재우면 62.5ms 만에 깨는 일이 잦고, 그러면 20Hz 로 짠 루프가 실제로는 16Hz 로
// 덜컹거린다. 고해상도 대기 타이머는 눈금이 100ns 다.
//
// 겸사겸사 실제 주기를 지켜본다. 틱이 늦은 초에만 한 줄 찍으므로 평소에는
// 조용하다. 틱이 밀리는 것과 타이머가 굵은 것은 화면에서 똑같이 "가끔 멈칫"
// 으로 보이는데, worst work 가 예산 안이면 타이머 쪽이고 넘으면 틱 쪽이다.
class TickPacer {
  public:
    TickPacer(std::chrono::nanoseconds period, const char* what)
        : what_(what), period_(period),
          expected_(static_cast<int>(std::chrono::seconds(1) / period)),
          timer_(::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS)),
          lastWake_(std::chrono::steady_clock::now()) {
        if (timer_ == nullptr) {
            spdlog::warn("{}: no high resolution timer, the tick will be coarse", what_);
        }
    }

    TickPacer(const TickPacer&) = delete;
    TickPacer& operator=(const TickPacer&) = delete;

    ~TickPacer() {
        if (timer_ != nullptr) {
            ::CloseHandle(timer_);
        }
    }

    // deadline 은 이번 틱이 시작한 시각 + period 다.
    void sleepUntil(std::chrono::steady_clock::time_point deadline) {
        const auto now = std::chrono::steady_clock::now();
        const auto work = period_ - (deadline - now);
        if (deadline > now) {
            if (!waitWith(timer_, deadline - now)) {
                std::this_thread::sleep_until(deadline);
            }
        }
        report(work);
    }

  private:
    static bool waitWith(HANDLE timer, std::chrono::steady_clock::duration remaining) {
        if (timer == nullptr) {
            return false;
        }
        LARGE_INTEGER due;
        // 음수는 상대 시간이고 단위는 100ns 다.
        due.QuadPart =
            -(std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count() / 100);
        if (!::SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
            return false;
        }
        ::WaitForSingleObject(timer, INFINITE);
        return true;
    }

    // 이 스레드가 지금까지 쓴 CPU 시간 (커널 + 유저).
    static std::chrono::nanoseconds threadCpu() {
        FILETIME creation{};
        FILETIME exited{};
        FILETIME kernel{};
        FILETIME user{};
        if (!::GetThreadTimes(::GetCurrentThread(), &creation, &exited, &kernel, &user)) {
            return {};
        }
        const auto ticks = [](const FILETIME& value) {
            return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
        };
        // FILETIME 의 눈금은 100ns 다.
        return std::chrono::nanoseconds((ticks(kernel) + ticks(user)) * 100);
    }

    void report(std::chrono::steady_clock::duration work) {
        const auto woke = std::chrono::steady_clock::now();
        const auto span = woke - lastWake_;
        lastWake_ = woke;

        // 대기는 CPU 를 쓰지 않으므로, 지난 깨어남 이후의 CPU 는 곧 틱 작업이
        // 실제로 돈 시간이다. 벽시계는 3초인데 CPU 가 0 이면 계산이 아니라
        // 막혀 있었거나 스케줄을 못 받은 것이다.
        const auto cpu = threadCpu();
        const auto cpuDelta = cpu - lastCpu_;
        lastCpu_ = cpu;

        ++ticks_;
        accumulated_ += span;
        worstSpan_ = std::max(worstSpan_, span);
        if (work > worstWork_) {
            worstWork_ = work;
            worstCpu_ = cpuDelta;
        }
        if (accumulated_ < std::chrono::seconds(1)) {
            return;
        }
        if (worstSpan_ > period_ + period_ / 4) {
            const auto ms = [](std::chrono::steady_clock::duration value) {
                return std::chrono::duration<double, std::milli>(value).count();
            };
            spdlog::warn(
                "{}: {} ticks last second (want {}), worst period {:.1f}ms, worst work {:.1f}ms "
                "(cpu {:.1f}ms)",
                what_, ticks_, expected_, ms(worstSpan_), ms(worstWork_), ms(worstCpu_));
        }
        ticks_ = 0;
        accumulated_ = {};
        worstSpan_ = {};
        worstWork_ = {};
        worstCpu_ = {};
    }

    const char* what_;
    std::chrono::nanoseconds period_;
    int expected_;
    HANDLE timer_;
    std::chrono::steady_clock::time_point lastWake_;
    std::chrono::steady_clock::duration accumulated_{};
    std::chrono::steady_clock::duration worstSpan_{};
    std::chrono::steady_clock::duration worstWork_{};
    std::chrono::nanoseconds worstCpu_{};
    std::chrono::nanoseconds lastCpu_{threadCpu()};
    int ticks_ = 0;
};

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
