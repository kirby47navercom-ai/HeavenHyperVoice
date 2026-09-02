# 서버 지형. 맵 파일(.hhvmap)을 읽어 navmesh 를 만들고, 그 위에서 이동
# 가능성과 경로를 답한다.
#
# 필드와 인스턴스가 같은 것을 쓴다. 지형 판정에는 게임 규칙이 없어서 —
# "여기 설 수 있는가" 는 필드든 사냥터든 같은 질문이다 — 갈라 둘 이유가 없다.
# 전투가 붙는 World 쪽은 서버마다 따로다.

find_package(recastnavigation CONFIG REQUIRED)

add_library(Nav STATIC
    src/Map.cpp
    src/Path.cpp
)

target_include_directories(Nav PUBLIC src)

target_link_libraries(Nav PUBLIC
    RecastNavigation::Recast
    RecastNavigation::Detour
)
