# 계정과 캐릭터 저장소.
#
# LoginServer 만 쓰던 것을 FieldServer 도 쓰게 되면서 밖으로 뺐다.
# 필드 서버는 입장할 때 캐릭터와 마지막 위치를 읽어야 한다.
#
# 이 라이브러리를 링크하는 프로세스는 DB 자격증명을 갖게 된다. 지금은 둘이고,
# 셋을 넘어가면 저장소를 별도 서비스로 빼는 편이 낫다 (README 참고).

find_package(unofficial-argon2 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_library(Data STATIC
    src/OdbcStore.cpp
    src/DevStore.cpp
    src/PasswordHash.cpp
)

target_include_directories(Data PUBLIC src)

# odbc32 는 ODBC 드라이버 매니저다. vcpkg 패키지가 아니라 Windows SDK 구성요소다.
target_link_libraries(Data PUBLIC
    Protocol
    spdlog::spdlog
    odbc32
)

target_link_libraries(Data PRIVATE
    unofficial::argon2::libargon2
)

# ODBC 헤더가 windows.h 를 끌고 들어온다. NOMINMAX 가 없으면 max/min 매크로가
# flatbuffers 의 std::numeric_limits<...>::max() 를 깨뜨린다.
target_compile_definitions(Data PUBLIC
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
