cmake_minimum_required(VERSION 3.20)

project(GRPCTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ============================================================
# gRPC installation
# ============================================================

set(gRPC_INSTALL_DIR "D:/grpc/install")

set(GRPC_INCLUDE_DIR
    "${gRPC_INSTALL_DIR}/include"
)

set(GRPC_LIB_DIR
    "${gRPC_INSTALL_DIR}/lib"
)


# ============================================================
# Proto file
# ============================================================

set(PROTO_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/enrollment.proto"
)

set(GENERATED_PROTO
    "${CMAKE_CURRENT_BINARY_DIR}/enrollment.pb.cc"
    "${CMAKE_CURRENT_BINARY_DIR}/enrollment.pb.h"
    "${CMAKE_CURRENT_BINARY_DIR}/enrollment.grpc.pb.cc"
    "${CMAKE_CURRENT_BINARY_DIR}/enrollment.grpc.pb.h"
)


# ============================================================
# Generate protobuf and gRPC files
# ============================================================

add_custom_command(
    OUTPUT ${GENERATED_PROTO}

    COMMAND "${gRPC_INSTALL_DIR}/bin/protoc.exe"
    ARGS
        --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
        --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
        ${PROTO_FILE}

    COMMAND "${gRPC_INSTALL_DIR}/bin/protoc.exe"
    ARGS
        --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
        --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
        --plugin=protoc-gen-grpc=${gRPC_INSTALL_DIR}/bin/grpc_cpp_plugin.exe
        ${PROTO_FILE}

    DEPENDS
        "${PROTO_FILE}"

    VERBATIM
)


# ============================================================
# Proto generation target
# ============================================================

add_custom_target(generate_proto
    DEPENDS ${GENERATED_PROTO}
)


# ============================================================
# Collect Abseil libraries
#
# All Abseil libraries are included because gRPC was built
# statically and several of its components depend on each other.
# ============================================================

file(GLOB ABSL_LIBRARIES
    "${GRPC_LIB_DIR}/libabsl_*.a"
)


# ============================================================
# Static gRPC dependency group
#
# --start-group / --end-group allows the linker to repeatedly
# search static libraries until all circular dependencies are
# resolved.
# ============================================================

set(GRPC_STATIC_LIBRARIES

    # --------------------------------------------------------
    # Main gRPC libraries
    # --------------------------------------------------------

    "${GRPC_LIB_DIR}/libgrpc++.a"
    "${GRPC_LIB_DIR}/libgrpc.a"
    "${GRPC_LIB_DIR}/libgpr.a"


    # --------------------------------------------------------
    # Protocol Buffers
    # --------------------------------------------------------

    "${GRPC_LIB_DIR}/libprotobuf.a"


    # --------------------------------------------------------
    # Abseil
    # --------------------------------------------------------

    ${ABSL_LIBRARIES}


    # --------------------------------------------------------
    # upb
    # --------------------------------------------------------

    "${GRPC_LIB_DIR}/libupb.a"
    "${GRPC_LIB_DIR}/libupb_base_lib.a"
    "${GRPC_LIB_DIR}/libupb_descriptor_lib.a"
    "${GRPC_LIB_DIR}/libupb_hash_lib.a"
    "${GRPC_LIB_DIR}/libupb_json_lib.a"
    "${GRPC_LIB_DIR}/libupb_lex_lib.a"
    "${GRPC_LIB_DIR}/libupb_mem_lib.a"
    "${GRPC_LIB_DIR}/libupb_message_lib.a"
    "${GRPC_LIB_DIR}/libupb_mini_descriptor_lib.a"
    "${GRPC_LIB_DIR}/libupb_mini_table_lib.a"
    "${GRPC_LIB_DIR}/libupb_reflection_lib.a"
    "${GRPC_LIB_DIR}/libupb_textformat_lib.a"
    "${GRPC_LIB_DIR}/libupb_wire_lib.a"


    # --------------------------------------------------------
    # Other gRPC dependencies
    # --------------------------------------------------------

    "${GRPC_LIB_DIR}/libaddress_sorting.a"
    "${GRPC_LIB_DIR}/libcares.a"
    "${GRPC_LIB_DIR}/libre2.a"
    "${GRPC_LIB_DIR}/libutf8_range.a"
    "${GRPC_LIB_DIR}/libutf8_range_lib.a"
    "${GRPC_LIB_DIR}/libutf8_validity.a"


    # --------------------------------------------------------
    # OpenSSL
    # --------------------------------------------------------

    "${GRPC_LIB_DIR}/libssl.a"
    "${GRPC_LIB_DIR}/libcrypto.a"


    # --------------------------------------------------------
    # zlib
    # --------------------------------------------------------

    "${GRPC_LIB_DIR}/libzlibstatic.a"
)


# ============================================================
# Function to link gRPC
# ============================================================

function(link_grpc_static TARGET)

    target_link_libraries(${TARGET} PRIVATE

        "-Wl,--start-group"

        ${GRPC_STATIC_LIBRARIES}

        "-Wl,--end-group"

        ws2_32
        advapi32
        iphlpapi
        user32
        gdi32
        shell32
        ole32
        oleaut32
        uuid
        comdlg32
        dbghelp
        crypt32
        bcrypt
    )

endfunction()


# ============================================================
# Server
# ============================================================

add_executable(server
    server.cpp
    ${GENERATED_PROTO}
)

add_dependencies(server generate_proto)

target_include_directories(server PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${GRPC_INCLUDE_DIR}"
)

link_grpc_static(server)


# ============================================================
# Client
# ============================================================

add_executable(client
    client.cpp
    ${GENERATED_PROTO}
)

add_dependencies(client generate_proto)

target_include_directories(client PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${GRPC_INCLUDE_DIR}"
)

link_grpc_static(client)

# ============================================================
# Web server
# ============================================================

add_executable(web_server
    web_server.cpp
    ${GENERATED_PROTO}
)

add_dependencies(web_server generate_proto)

target_include_directories(web_server PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${GRPC_INCLUDE_DIR}"
)

link_grpc_static(web_server)