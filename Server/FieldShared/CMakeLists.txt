add_library(FieldShared STATIC
    src/PartnerFollower.cpp
    src/PokemonMovement.cpp
)

target_include_directories(FieldShared PUBLIC src)

target_link_libraries(FieldShared PUBLIC
    Nav
)
