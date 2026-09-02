# 야생 포켓몬과 그 Lua AI 는 InstanceServer 로 갔다. 필드는 플레이어와
# 파트너만 있고 전투도 없어서 Lua 를 링크하지 않는다.
add_executable(FieldServer
    src/main.cpp
    src/FieldHandler.cpp
    src/World.cpp
    src/MapCollision.cpp
)

target_include_directories(FieldServer PRIVATE src)

# DB 비밀번호는 LoginServer 와 같은 자격증명에서 읽으므로 추가 설정이 없다.
# 그 API(advapi32)는 Net 이 들고 온다.
target_link_libraries(FieldServer PRIVATE
    Net
    Data
)

target_compile_definitions(FieldServer PRIVATE
    # 개발용 인증서/키를 찾기 위한 폴백 경로. Debug 에만 넣는다.
    $<$<CONFIG:Debug>:HHV_DEV_ROOT="${PROJECT_SOURCE_DIR}">
)
