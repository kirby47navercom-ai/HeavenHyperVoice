find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)

add_executable(FieldServer
    src/main.cpp
    src/FieldHandler.cpp
    src/World.cpp
    src/WildAi.cpp
    src/MapCollision.cpp
)

target_include_directories(FieldServer PRIVATE src)

# advapi32 는 자격증명 관리자(CredRead) API 다. DB 비밀번호를 LoginServer 와
# 같은 자격증명에서 읽으므로 추가 설정이 없다.
target_link_libraries(FieldServer PRIVATE
    Net
    Data
    sol2::sol2
    ${LUA_LIBRARIES}
    advapi32
)
target_include_directories(FieldServer PRIVATE ${LUA_INCLUDE_DIR})

target_compile_definitions(FieldServer PRIVATE
    # 개발용 인증서/키를 찾기 위한 폴백 경로. Debug 에만 넣는다.
    $<$<CONFIG:Debug>:HHV_DEV_ROOT="${PROJECT_SOURCE_DIR}">
)

# Lua 행동 스크립트를 실행 파일 옆으로 복사한다. 서버는 작업 디렉터리 기준
# scripts/wild_ai.lua 로 찾으므로, 런처/직접 실행 모두 별도 설정 없이 뜬다.
add_custom_command(TARGET FieldServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
        "$<TARGET_FILE_DIR:FieldServer>/scripts"
    COMMENT "Copying wild AI scripts next to FieldServer"
)
