find_package(recastnavigation CONFIG REQUIRED)

add_library(ServerMovement STATIC
    src/Map.cpp
    src/Path.cpp
    src/NavigationMesh.cpp
)

target_link_libraries(ServerMovement PUBLIC RecastNavigation::Recast RecastNavigation::Detour)

target_include_directories(ServerMovement PUBLIC
    src
    "${PROJECT_SOURCE_DIR}/../Client/Source/MovementCore"
)
