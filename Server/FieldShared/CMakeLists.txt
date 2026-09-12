add_library(FieldShared STATIC
    src/PartnerFollower.cpp
    src/PokemonMovement.cpp
)

target_include_directories(FieldShared PUBLIC src)

# PartyState.h 는 저장소를 읽고 세션으로 내보낸다. 헤더만 있는 파일이지만
# 링크하는 쪽이 이미 넷을 다 걸고 있어 PUBLIC 으로 둔다.
target_link_libraries(FieldShared PUBLIC
    ServerMovement
    Net
    Data
)
