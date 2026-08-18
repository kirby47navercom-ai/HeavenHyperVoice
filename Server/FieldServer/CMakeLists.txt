add_executable(FieldServer
    src/main.cpp
    src/FieldHandler.cpp
    src/World.cpp
    src/MapCollision.cpp
)

target_include_directories(FieldServer PRIVATE src)

# advapi32 는 자격증명 관리자(CredRead) API 다. DB 비밀번호를 LoginServer 와
# 같은 자격증명에서 읽으므로 추가 설정이 없다.
target_link_libraries(FieldServer PRIVATE
    Net
    Data
    advapi32
)

target_compile_definitions(FieldServer PRIVATE
    # 개발용 인증서/키를 찾기 위한 폴백 경로. Debug 에만 넣는다.
    $<$<CONFIG:Debug>:HHV_DEV_ROOT="${PROJECT_SOURCE_DIR}">
)
