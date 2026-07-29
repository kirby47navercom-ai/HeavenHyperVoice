add_executable(TestClient
    src/main.cpp
)

target_link_libraries(TestClient PRIVATE Net)
