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
