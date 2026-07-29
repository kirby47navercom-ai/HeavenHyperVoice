#pragma once

// 인증서/키 파일 경로 해석. 서버들이 공유한다.
//
// 상대 경로는 현재 작업 디렉터리 기준으로 풀린다. IDE 는 보통 작업 디렉터리가
// 빌드 출력 폴더라 개발용 파일을 못 찾는다. Debug 빌드에 한해 빌드 시점에 박아둔
// 소스 루트를 폴백으로 쓰고, 쓰이면 경고를 남긴다.
//
// HHV_DEV_ROOT 는 각 실행 파일의 CMakeLists 에서 $<$<CONFIG:Debug>:...> 로 주입한다.

#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace heaven::net {

inline std::string resolveResourcePath(const std::string& given, const char* what) {
    std::error_code ec;
    std::string tried;

    const std::filesystem::path direct = std::filesystem::absolute(given, ec);
    if (!ec) {
        if (std::filesystem::exists(direct, ec)) {
            return direct.string();
        }
        tried += "\n                      " + direct.string();
    }

#ifdef HHV_DEV_ROOT
    if (!std::filesystem::path(given).is_absolute()) {
        const std::filesystem::path devPath =
            (std::filesystem::path(HHV_DEV_ROOT) / given).lexically_normal();
        if (std::filesystem::exists(devPath, ec)) {
            spdlog::warn("{} not found from the working directory, using dev fallback: {}", what,
                         devPath.string());
            return devPath.string();
        }
        tried += "\n                      " + devPath.string();
    }
#endif

    throw std::runtime_error(
        std::string(what) + " not found\n"
        "  tried             :" + tried + "\n"
        "  working directory : " + std::filesystem::current_path().string() + "\n"
        "  fix               : generate dev files with the scripts in tools\\, or pass\n"
        "                      an absolute path on the command line.");
}

}  // namespace heaven::net
