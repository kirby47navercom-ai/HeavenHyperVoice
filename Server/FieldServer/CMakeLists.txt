find_package(recastnavigation CONFIG REQUIRED)

add_executable(FieldServer
    src/main.cpp
    src/FieldHandler.cpp
    src/World.cpp
    src/WildAi.cpp
    src/Map.cpp
    src/Path.cpp
)

target_include_directories(FieldServer PRIVATE src)

# DB 비밀번호는 LoginServer 와 같은 자격증명에서 읽으므로 추가 설정이 없다.
# 그 API(advapi32)는 Net 이 들고 온다.
target_link_libraries(FieldServer PRIVATE
    Net
    Data
    RecastNavigation::Recast
    RecastNavigation::Detour
)

target_compile_definitions(FieldServer PRIVATE
    # 개발용 인증서/키를 찾기 위한 폴백 경로. Debug 에만 넣는다.
    $<$<CONFIG:Debug>:HHV_DEV_ROOT="${PROJECT_SOURCE_DIR}">
)

add_custom_command(TARGET FieldServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/maps"
        "$<TARGET_FILE_DIR:FieldServer>/maps"
    COMMENT "Copying field maps next to FieldServer"
)
