add_library(ServerMovement STATIC
    src/Map.cpp
    src/Path.cpp
)

target_include_directories(ServerMovement PUBLIC
    src
    "${PROJECT_SOURCE_DIR}/../Client/Source/MovementCore"
)
