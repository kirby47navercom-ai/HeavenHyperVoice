find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)

# World 와 MapCollision 은 FieldServer 것과 같은 자리에서 갈라져 나온 사본이다.
# 필드는 야생도 전투도 없어서 그쪽 World 에는 야생 부분이 없다. 전투가 붙을
# 곳이 여기라 한 벌로 묶지 않았다 — 대신 이동 검증이나 시야 판정처럼 양쪽에
# 다 있는 부분을 고칠 때는 반대쪽도 같이 봐야 한다.
add_executable(InstanceServer
    src/main.cpp
    src/InstanceHandler.cpp
    src/RoomManager.cpp
    src/World.cpp
    src/WildAi.cpp
    src/MapCollision.cpp
)

target_include_directories(InstanceServer PRIVATE src)

# DB 비밀번호는 LoginServer 와 같은 자격증명에서 읽으므로 추가 설정이 없다.
# 그 API(advapi32)는 Net 이 들고 온다.
target_link_libraries(InstanceServer PRIVATE
    Net
    Data
    sol2::sol2
    ${LUA_LIBRARIES}
)
target_include_directories(InstanceServer PRIVATE ${LUA_INCLUDE_DIR})

target_compile_definitions(InstanceServer PRIVATE
    # 개발용 인증서/키를 찾기 위한 폴백 경로. Debug 에만 넣는다.
    $<$<CONFIG:Debug>:HHV_DEV_ROOT="${PROJECT_SOURCE_DIR}">
)

# Lua 행동 스크립트를 실행 파일 옆으로 복사한다. 서버는 작업 디렉터리 기준
# scripts/wild_ai.lua 로 찾으므로, 런처/직접 실행 모두 별도 설정 없이 뜬다.
add_custom_command(TARGET InstanceServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
        "$<TARGET_FILE_DIR:InstanceServer>/scripts"
    COMMENT "Copying wild AI scripts next to InstanceServer"
)
