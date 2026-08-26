find_package(flatbuffers CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

# 스키마를 추가하면 여기에만 넣으면 된다.
set(_schemas
    chat.fbs
    login.fbs
    auth.fbs
    field.fbs
)

set(_gen_headers "")
foreach(_schema IN LISTS _schemas)
    get_filename_component(_stem "${_schema}" NAME_WE)
    set(_header "${_gen_dir}/${_stem}_generated.h")
    list(APPEND _gen_headers "${_header}")

    # *.fbs -> *_generated.h (빌드 시점 코드젠, 생성 헤더는 커밋하지 않는다)
    add_custom_command(
        OUTPUT "${_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_gen_dir}"
        COMMAND flatbuffers::flatc --cpp --scoped-enums -o "${_gen_dir}"
                "${CMAKE_CURRENT_SOURCE_DIR}/${_schema}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_schema}" flatbuffers::flatc
        COMMENT "flatc: ${_schema} -> ${_stem}_generated.h"
        VERBATIM
    )
endforeach()

add_custom_target(ProtocolGen DEPENDS ${_gen_headers})

# 언리얼 클라이언트는 flatc 를 돌리지 않는다. 팀원이 서버를 빌드하지 않아도
# 클라가 컴파일돼야 하므로 클라가 쓰는 스키마의 생성 헤더만 클라 트리에 두고 커밋한다.
#
# 여기서 매 빌드 덮어쓴다. *.fbs 를 고치고 클라 쪽을 안 맞추면 git 에
# 수정된 파일로 뜨므로, 스키마가 조용히 어긋나는 일은 없다.
#
# auth.fbs 는 넣지 않는다. 클라이언트에게 티켓은 그대로 실어 보내는 불투명한
# 바이트열이고, 안을 들여다볼 이유가 없다.
set(_client_gen_dir "${PROJECT_SOURCE_DIR}/../Client/Source/HeavenHyperVoice/Net/Generated")

foreach(_client_schema field login)
    add_custom_command(TARGET ProtocolGen POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_gen_dir}/${_client_schema}_generated.h"
                "${_client_gen_dir}/${_client_schema}_generated.h"
        COMMENT "${_client_schema}_generated.h -> Client"
        VERBATIM
    )
endforeach()

add_library(Protocol STATIC
    AuthTicket.cpp
)

add_dependencies(Protocol ProtocolGen)

target_include_directories(Protocol PUBLIC
    "${_gen_dir}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(Protocol PUBLIC
    flatbuffers::flatbuffers
    OpenSSL::Crypto
)
