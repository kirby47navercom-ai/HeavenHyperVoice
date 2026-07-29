cmake_minimum_required(VERSION 3.28)

# vcpkg 툴체인 자동 탐지.
# 이렇게 해두면 Rider/CLion/VS/CLI 어디서 열든 VCPKG_ROOT 환경변수 없이도 configure 된다.
# 명시적으로 -DCMAKE_TOOLCHAIN_FILE=... 을 주면 그 값이 우선한다.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(_vcpkg_root "")
    if(DEFINED ENV{VCPKG_ROOT})
        set(_vcpkg_root "$ENV{VCPKG_ROOT}")
    else()
        foreach(_candidate "C:/tools/vcpkg" "C:/vcpkg" "$ENV{USERPROFILE}/vcpkg")
            if(EXISTS "${_candidate}/scripts/buildsystems/vcpkg.cmake")
                set(_vcpkg_root "${_candidate}")
                break()
            endif()
        endforeach()
    endif()

    if(EXISTS "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake"
            CACHE FILEPATH "vcpkg toolchain")
        message(STATUS "vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
    else()
        message(FATAL_ERROR
            "vcpkg not found. Set the VCPKG_ROOT environment variable or pass "
            "-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake")
    endif()
endif()

project(HHVServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
endif()

add_subdirectory(Protocol)
add_subdirectory(Net)
add_subdirectory(LoginServer)
add_subdirectory(ChatServer)
add_subdirectory(TestClient)
add_subdirectory(Launcher)

