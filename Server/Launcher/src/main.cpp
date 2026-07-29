// Launcher - 서버들을 한꺼번에 띄운다.
//
// 자식 프로세스를 Job Object 에 넣고 JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE 를 건다.
// 런처가 어떤 이유로 죽든(정상 종료, Ctrl+C, 강제 종료) 커널이 자식을 함께 정리하므로
// 고아 서버가 남지 않는다. 개발 중 포트가 물려 있는 사고를 막아준다.

#include <windows.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Options {
    std::uint16_t loginPort = 9100;
    std::uint16_t chatPort = 9000;
    std::string chatHost = "127.0.0.1";
    bool verbose = false;
};

struct Child {
    std::string name;
    PROCESS_INFORMATION info{};
};

HANDLE g_job = nullptr;

void printUsage() {
    std::cout << "Launcher - starts the HeavenHyperVoice servers together\n"
                 "\n"
                 "  --login-port <n>  login server port (default 9100)\n"
                 "  --chat-port <n>   chat server port (default 9000)\n"
                 "  --chat-host <h>   chat host advertised to clients (default 127.0.0.1)\n"
                 "  --verbose         pass --verbose to the servers\n"
                 "  --help            show this message\n"
                 "\n"
                 "Servers are looked up next to this executable. Children are placed in a job\n"
                 "object, so they are terminated whenever the launcher exits.\n";
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

        if (arg == "--login-port") {
            options.loginPort = static_cast<std::uint16_t>(std::stoi(next("--login-port")));
        } else if (arg == "--chat-port") {
            options.chatPort = static_cast<std::uint16_t>(std::stoi(next("--chat-port")));
        } else if (arg == "--chat-host") {
            options.chatHost = next("--chat-host");
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

std::filesystem::path executableDirectory() {
    wchar_t buffer[MAX_PATH];
    const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        throw std::runtime_error("GetModuleFileNameW failed");
    }
    return std::filesystem::path(buffer).parent_path();
}

BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\n[launcher] shutting down, terminating servers" << std::endl;
        if (g_job != nullptr) {
            ::TerminateJobObject(g_job, 0);
        }
        return TRUE;
    }
    return FALSE;
}

Child startServer(const std::filesystem::path& exe, const std::string& commandLine,
                  const std::string& name) {
    if (!std::filesystem::exists(exe)) {
        throw std::runtime_error("server executable not found: " + exe.string() +
                                 "\n  (build all targets first: cmake --build --preset debug)");
    }

    // CreateProcessW 는 커맨드라인을 수정할 수 있으므로 쓰기 가능한 버퍼가 필요하다.
    std::wstring wide(commandLine.begin(), commandLine.end());
    std::vector<wchar_t> mutableCommandLine(wide.begin(), wide.end());
    mutableCommandLine.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);

    Child child;
    child.name = name;

    // CREATE_SUSPENDED 로 만들어 Job 에 넣은 뒤 재개한다.
    // 그래야 자식이 Job 밖에서 손자를 만들 틈이 없다.
    if (!::CreateProcessW(exe.wstring().c_str(), mutableCommandLine.data(), nullptr, nullptr,
                          FALSE, CREATE_SUSPENDED, nullptr, nullptr, &startup, &child.info)) {
        throw std::runtime_error("CreateProcess failed for " + name + " (error " +
                                 std::to_string(::GetLastError()) + ")");
    }

    if (!::AssignProcessToJobObject(g_job, child.info.hProcess)) {
        ::TerminateProcess(child.info.hProcess, 1);
        ::CloseHandle(child.info.hThread);
        ::CloseHandle(child.info.hProcess);
        throw std::runtime_error("AssignProcessToJobObject failed for " + name);
    }

    ::ResumeThread(child.info.hThread);
    std::cout << "[launcher] started " << name << " (pid " << child.info.dwProcessId << ')'
              << std::endl;
    return child;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<Child> children;
    try {
        const Options options = parseArgs(argc, argv);
        const std::filesystem::path dir = executableDirectory();

        g_job = ::CreateJobObjectW(nullptr, nullptr);
        if (g_job == nullptr) {
            throw std::runtime_error("CreateJobObject failed");
        }

        // 런처가 사라지면 Job 핸들이 닫히고, 커널이 자식을 모두 종료한다.
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!::SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &limits,
                                       sizeof(limits))) {
            throw std::runtime_error("SetInformationJobObject failed");
        }

        ::SetConsoleCtrlHandler(consoleHandler, TRUE);

        const std::string verbose = options.verbose ? " --verbose" : "";

        const std::filesystem::path loginExe = dir / "LoginServer.exe";
        const std::string loginArgs = "LoginServer --port " + std::to_string(options.loginPort) +
                                      " --chat-host " + options.chatHost + " --chat-port " +
                                      std::to_string(options.chatPort) + verbose;

        const std::filesystem::path chatExe = dir / "ChatServer.exe";
        const std::string chatArgs =
            "ChatServer --port " + std::to_string(options.chatPort) + verbose;

        children.push_back(startServer(loginExe, loginArgs, "LoginServer"));
        children.push_back(startServer(chatExe, chatArgs, "ChatServer"));

        std::cout << "[launcher] login on " << options.loginPort << ", chat on "
                  << options.chatPort << ". Ctrl+C to stop both." << std::endl;

        // 아무 자식이나 죽으면 나머지도 정리한다. 반쪽만 살아있는 상태를 만들지 않는다.
        std::vector<HANDLE> handles;
        handles.reserve(children.size());
        for (const Child& child : children) {
            handles.push_back(child.info.hProcess);
        }

        const DWORD result = ::WaitForMultipleObjects(static_cast<DWORD>(handles.size()),
                                                      handles.data(), FALSE, INFINITE);
        const std::size_t index = result - WAIT_OBJECT_0;
        if (index < children.size()) {
            DWORD exitCode = 0;
            ::GetExitCodeProcess(children[index].info.hProcess, &exitCode);
            std::cout << "[launcher] " << children[index].name << " exited with code " << exitCode
                      << ", stopping the rest" << std::endl;
        }

        ::TerminateJobObject(g_job, 0);

        for (Child& child : children) {
            ::CloseHandle(child.info.hThread);
            ::CloseHandle(child.info.hProcess);
        }
        ::CloseHandle(g_job);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[launcher] fatal: " << e.what() << std::endl;
        if (g_job != nullptr) {
            ::TerminateJobObject(g_job, 1);
            ::CloseHandle(g_job);
        }
        return 1;
    }
}
