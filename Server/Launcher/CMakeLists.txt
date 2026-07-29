# 런처는 서버들을 프로세스로 띄우기만 하므로 Net/Protocol 에 의존하지 않는다.
add_executable(Launcher
    src/main.cpp
)

target_compile_definitions(Launcher PRIVATE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# 런처가 서버 exe 를 자기 옆에서 찾으므로 출력 위치를 맞춰준다.
set_target_properties(Launcher LoginServer ChatServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
