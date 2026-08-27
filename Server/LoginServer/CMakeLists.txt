add_executable(LoginServer
    src/main.cpp
    src/LoginHandler.cpp
)

target_include_directories(LoginServer PRIVATE src)

# 자격증명 관리자(advapi32)는 Net 이, ODBC 와 argon2 는 Data 가 들고 온다.
target_link_libraries(LoginServer PRIVATE
    Net
    Data
)

target_compile_definitions(LoginServer PRIVATE
    # 개발용 인증서/키를 찾기 위한 폴백 경로. Debug 에만 넣는다.
    $<$<CONFIG:Debug>:HHV_DEV_ROOT="${PROJECT_SOURCE_DIR}">
)
