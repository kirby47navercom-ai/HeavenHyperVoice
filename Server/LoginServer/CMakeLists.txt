add_executable(LoginServer
    src/main.cpp
    src/LoginHandler.cpp
    src/DevAccountStore.cpp
)

target_include_directories(LoginServer PRIVATE src)

target_link_libraries(LoginServer PRIVATE Net)

target_compile_definitions(LoginServer PRIVATE
    # 개발용 인증서/키를 찾기 위한 폴백 경로. Debug 에만 넣는다.
    $<$<CONFIG:Debug>:HHV_DEV_ROOT="${PROJECT_SOURCE_DIR}">
)
