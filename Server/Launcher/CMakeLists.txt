# 런처는 서버들을 프로세스로 띄우기만 하므로 Net/Protocol 에 의존하지 않는다.
add_executable(Launcher
    src/main.cpp
)

target_compile_definitions(Launcher PRIVATE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# 링크하지 않으므로 CMake 가 의존 관계를 알지 못한다. 명시해두지 않으면
# IDE 에서 Launcher 만 빌드해 실행했을 때 서버 exe 가 없어 실패한다.
add_dependencies(Launcher LoginServer ChatServer FieldServer)
