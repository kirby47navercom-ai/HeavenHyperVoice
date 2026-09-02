# 인스턴스 서버. 사냥터와 레이드를 방 단위로 돌린다.
#
# World 는 FieldServer 것과 같은 자리에서 갈라져 나온 사본이다. 전투가 붙을
# 곳이 여기라 한 벌로 묶지 않았다. 반대로 지형(Nav)에는 게임 규칙이 없어서
# 두 서버가 같은 것을 쓴다.

add_executable(InstanceServer
    src/main.cpp
    src/InstanceHandler.cpp
    src/RoomManager.cpp
    src/World.cpp
    src/WildAi.cpp
)

target_include_directories(InstanceServer PRIVATE src)

target_link_libraries(InstanceServer PRIVATE
    Net
    Data
    Nav
)

target_compile_definitions(InstanceServer PRIVATE
    # 개발용 인증서/키를 찾기 위한 폴백 경로. Debug 에만 넣는다.
    $<$<CONFIG:Debug>:HHV_DEV_ROOT="${PROJECT_SOURCE_DIR}">
)

add_custom_command(TARGET InstanceServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/maps"
        "$<TARGET_FILE_DIR:InstanceServer>/maps"
    COMMENT "Copying instance maps next to InstanceServer"
)
