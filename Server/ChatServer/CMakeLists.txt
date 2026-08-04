add_executable(ChatServer
    src/main.cpp
    src/ChatHandler.cpp
    src/Room.cpp
    src/SessionRegistry.cpp
)

target_include_directories(ChatServer PRIVATE src)

target_link_libraries(ChatServer PRIVATE Net)

target_compile_definitions(ChatServer PRIVATE
    # 개발용 인증서를 찾기 위한 폴백 경로. IDE 는 작업 디렉터리가 빌드 출력
    # 폴더라 상대 경로가 안 맞는다. Debug 에만 넣어 릴리스 바이너리에는
    # 개발 머신 경로가 남지 않게 한다.
    $<$<CONFIG:Debug>:HHV_DEV_ROOT="${PROJECT_SOURCE_DIR}">
)
