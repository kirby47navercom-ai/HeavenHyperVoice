find_package(unofficial-argon2 CONFIG REQUIRED)

add_executable(LoginServer
    src/main.cpp
    src/LoginHandler.cpp
    src/DevStore.cpp
    src/OdbcStore.cpp
    src/PasswordHash.cpp
    src/WorkQueue.cpp
)

target_include_directories(LoginServer PRIVATE src)

# odbc32 는 ODBC 드라이버 매니저, advapi32 는 자격증명 관리자(CredRead/CredWrite) API 다.
# 둘 다 vcpkg 패키지가 아니라 Windows SDK 구성요소다.
target_link_libraries(LoginServer PRIVATE
    Net
    unofficial::argon2::libargon2
    odbc32
    advapi32
)

target_compile_definitions(LoginServer PRIVATE
    # 개발용 인증서/키를 찾기 위한 폴백 경로. Debug 에만 넣는다.
    $<$<CONFIG:Debug>:HHV_DEV_ROOT="${PROJECT_SOURCE_DIR}">
)
