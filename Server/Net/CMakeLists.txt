find_package(OpenSSL REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)

# IOCP 를 직접 쓰므로 Windows 전용이다.
if(NOT WIN32)
    message(FATAL_ERROR "Net is built on IOCP and only supports Windows.")
endif()

add_library(Net STATIC
    src/Tls.cpp
    src/TlsSession.cpp
    src/TlsServer.cpp
    src/RedisClient.cpp
    src/WorkQueue.cpp
    src/Credentials.cpp
)

target_include_directories(Net PUBLIC src)

target_link_libraries(Net PUBLIC
    Protocol
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
    ws2_32
    mswsock
    advapi32
    hiredis::hiredis
)

target_compile_definitions(Net PUBLIC
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
