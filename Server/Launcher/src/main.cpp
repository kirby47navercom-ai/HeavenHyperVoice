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
    std::uint16_t fieldPort = 9200;
    std::uint16_t instancePort = 9300;
    // 셋 다 같은 머신에서 뜨므로 주소도 하나다. 나뉘면 그때 쪼갠다.
    std::string host = "127.0.0.1";

    // 인스턴스 방 하나에 뿌릴 야생 포켓몬 수. 야생은 필드에서 인스턴스로
    // 옮겨갔다. 배회 구역이 월드 중앙 8000x8000 이라 기본값보다 넉넉히 둔다.
    int wildCount = 50;

    bool verbose = false;
};

struct Child {
    std::string name;
    PROCESS_INFORMATION info{};
};

HANDLE g_job = nullptr;

// 띄운 자식들의 프로세스 핸들. 시작이 끝나면 더 바뀌지 않는다.
std::vector<HANDLE> g_processes;

// 자식이 스스로 정리할 때까지 기다리는 시간.
//
// 자식은 CREATE_NEW_PROCESS_GROUP 없이 만들어져 같은 콘솔에 붙는다. 그래서
// Ctrl+C 는 런처와 자식 셋에 **함께** 전달되고, 각 서버는 자기 콘솔 핸들러로
// 정상 종료를 시작한다. 여기서 바로 TerminateJobObject 를 부르면 그 정리가
// 잘려나간다 — FieldServer 는 그때 접속자 전원의 위치를 DB 에 저장한다.
//
// CTRL_CLOSE_EVENT 는 시스템이 약 5초 뒤 프로세스를 강제 종료하므로 그 안쪽으로 둔다.
//
// ponytail: 고정 4초. 접속자가 수백 명이 되면 위치 저장이 이 안에 안 끝날 수
//           있다. 그때는 자식이 "저장 끝났다" 를 알리게 하고 그걸 기다리면 된다.
constexpr DWORD kGracefulExitMs = 4000;

// 남은 자식이 모두 끝나기를 기다린 뒤, 그래도 살아 있으면 커널에 맡긴다.
void stopChildren() {
    if (!g_processes.empty()) {
        ::WaitForMultipleObjects(static_cast<DWORD>(g_processes.size()), g_processes.data(),
                                 TRUE, kGracefulExitMs);
    }
    if (g_job != nullptr) {
        ::TerminateJobObject(g_job, 0);
    }
}

void printUsage() {
    std::cout << "Launcher - starts the HeavenHyperVoice servers together\n"
                 "\n"
                 "  --login-port <n>  login server port (default 9100)\n"
                 "  --chat-port <n>   chat server port (default 9000)\n"
                 "  --field-port <n>  field server port (default 9200)\n"
                 "  --instance-port <n>  instance server port (default 9300)\n"
                 "  --host <h>        host advertised to clients for chat, field and\n"
                 "                    instances (default 127.0.0.1)\n"
                 "  --wild-count <n>  wild pokemon per instance room (default 50; higher\n"
                 "                    than the instance server's own default because they\n"
                 "                    roam only the middle 8000x8000 of the room)\n"
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
        } else if (arg == "--field-port") {
            options.fieldPort = static_cast<std::uint16_t>(std::stoi(next("--field-port")));
        } else if (arg == "--instance-port") {
            options.instancePort =
                static_cast<std::uint16_t>(std::stoi(next("--instance-port")));
        } else if (arg == "--host") {
            options.host = next("--host");
        } else if (arg == "--wild-count") {
            options.wildCount = std::stoi(next("--wild-count"));
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
        // 자식들도 같은 콘솔 신호를 받아 이미 정리를 시작했다. 기다렸다가 끊는다.
        std::cout << "\n[launcher] shutting down, waiting for servers to save and exit"
                  << std::endl;
        stopChildren();
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
    g_processes.push_back(child.info.hProcess);
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
                                      " --chat-host " + options.host + " --chat-port " +
                                      std::to_string(options.chatPort) + " --field-host " +
                                      options.host + " --field-port " +
                                      std::to_string(options.fieldPort) + " --instance-host " +
                                      options.host + " --instance-port " +
                                      std::to_string(options.instancePort) + verbose;

        const std::filesystem::path chatExe = dir / "ChatServer.exe";
        const std::string chatArgs =
            "ChatServer --port " + std::to_string(options.chatPort) + verbose;

        // 야생은 여기 없다. 필드는 플레이어와 파트너만 있고 전투도 없다.
        const std::filesystem::path fieldExe = dir / "FieldServer.exe";
        const std::string fieldArgs =
            "FieldServer --port " + std::to_string(options.fieldPort) + verbose;

        const std::filesystem::path instanceExe = dir / "InstanceServer.exe";
        const std::string instanceArgs = "InstanceServer --port " +
                                         std::to_string(options.instancePort) +
                                         " --wild-per-room " +
                                         std::to_string(options.wildCount) + verbose;

        children.push_back(startServer(loginExe, loginArgs, "LoginServer"));
        children.push_back(startServer(chatExe, chatArgs, "ChatServer"));
        children.push_back(startServer(fieldExe, fieldArgs, "FieldServer"));
        children.push_back(startServer(instanceExe, instanceArgs, "InstanceServer"));

        std::cout << "[launcher] login on " << options.loginPort << ", chat on "
                  << options.chatPort << ", field on " << options.fieldPort
                  << ", instances on " << options.instancePort
                  << ". Ctrl+C to stop all." << std::endl;

        // 아무 자식이나 죽으면 나머지도 정리한다. 반쪽만 살아있는 상태를 만들지 않는다.
        const DWORD result = ::WaitForMultipleObjects(static_cast<DWORD>(g_processes.size()),
                                                      g_processes.data(), FALSE, INFINITE);
        const std::size_t index = result - WAIT_OBJECT_0;
        if (index < children.size()) {
            DWORD exitCode = 0;
            ::GetExitCodeProcess(children[index].info.hProcess, &exitCode);
            std::cout << "[launcher] " << children[index].name << " exited with code " << exitCode
                      << ", stopping the rest" << std::endl;
        }

        // 여기서도 즉시 죽이지 않는다. Ctrl+C 로 셋이 함께 내려가는 중일 수 있고,
        // 그때 FieldServer 는 아직 위치를 저장하고 있다.
        stopChildren();

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
