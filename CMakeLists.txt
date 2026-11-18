cmake_minimum_required(VERSION 3.20)

project(DistributedEnrollment)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ============================================================
# Paths
# ============================================================

set(GRPC_INSTALL_DIR "D:/grpc/install")

set(GRPC_INCLUDE_DIR
    "${GRPC_INSTALL_DIR}/include"
)

set(GRPC_LIBRARY_DIR
    "${GRPC_INSTALL_DIR}/lib"
)

set(SQLITE_INCLUDE_DIR
    "D:/MSYS2/ucrt64/include"
)

set(SQLITE_LIBRARY
    "D:/MSYS2/ucrt64/lib/libsqlite3.a"
)


# ============================================================
# Threads
# ============================================================

find_package(Threads REQUIRED)


# ============================================================
# Proto generation
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


add_custom_command(
    OUTPUT ${GENERATED_PROTO}

    COMMAND
        "${GRPC_INSTALL_DIR}/bin/protoc.exe"

        --proto_path="${CMAKE_CURRENT_SOURCE_DIR}"

        --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"

        "${PROTO_FILE}"

    COMMAND
        "${GRPC_INSTALL_DIR}/bin/protoc.exe"

        --proto_path="${CMAKE_CURRENT_SOURCE_DIR}"

        --grpc_out="${CMAKE_CURRENT_BINARY_DIR}"

        --plugin=protoc-gen-grpc="${GRPC_INSTALL_DIR}/bin/grpc_cpp_plugin.exe"

        "${PROTO_FILE}"

    DEPENDS
        "${PROTO_FILE}"
)


add_custom_target(
    generate_proto
    DEPENDS ${GENERATED_PROTO}
)


# ============================================================
# Generated gRPC library
#
# Compile the generated protobuf/gRPC source ONCE.
# ============================================================

add_library(
    enrollment_proto
    STATIC

    "${CMAKE_CURRENT_BINARY_DIR}/enrollment.pb.cc"
    "${CMAKE_CURRENT_BINARY_DIR}/enrollment.grpc.pb.cc"
)


add_dependencies(
    enrollment_proto
    generate_proto
)


target_include_directories(
    enrollment_proto
    PUBLIC

    "${CMAKE_CURRENT_BINARY_DIR}"
    "${GRPC_INCLUDE_DIR}"
)


# ============================================================
# Find all Abseil libraries
#
# Protobuf/gRPC depend on many Abseil libraries.
# Instead of manually guessing which ones are needed,
# collect every static Abseil library from the installation.
# ============================================================

file(
    GLOB
    ABSL_LIBRARIES
    "${GRPC_LIBRARY_DIR}/libabsl_*.a"
)


# ============================================================
# gRPC / Protobuf libraries
# ============================================================

set(GRPC_LIBRARIES

    "${GRPC_LIBRARY_DIR}/libgrpc++.a"

    "${GRPC_LIBRARY_DIR}/libgrpc.a"

    "${GRPC_LIBRARY_DIR}/libgpr.a"

    "${GRPC_LIBRARY_DIR}/libaddress_sorting.a"

    "${GRPC_LIBRARY_DIR}/libupb.a"

    "${GRPC_LIBRARY_DIR}/libprotobuf.a"

)


# ============================================================
# Common gRPC linking
# ============================================================

function(link_grpc TARGET_NAME)

    target_link_libraries(
        ${TARGET_NAME}

        PRIVATE

        enrollment_proto

        ${GRPC_LIBRARIES}

        ${ABSL_LIBRARIES}

        ws2_32

        advapi32

        iphlpapi

        bcrypt

        crypt32

        Threads::Threads
    )

endfunction()


# ============================================================
# Server
# ============================================================

add_executable(
    server
    server.cpp
)


target_include_directories(
    server

    PRIVATE

    "${CMAKE_CURRENT_BINARY_DIR}"

    "${GRPC_INCLUDE_DIR}"

    "${SQLITE_INCLUDE_DIR}"
)


link_grpc(server)


target_link_libraries(
    server

    PRIVATE

    "${SQLITE_LIBRARY}"
)


# ============================================================
# Client
# ============================================================

add_executable(
    client
    client.cpp
)


target_include_directories(
    client

    PRIVATE

    "${CMAKE_CURRENT_BINARY_DIR}"

    "${GRPC_INCLUDE_DIR}"
)


link_grpc(client)


# ============================================================
# Web server
# ============================================================

add_executable(
    web_server
    web_server.cpp
)


target_include_directories(
    web_server

    PRIVATE

    "${CMAKE_CURRENT_BINARY_DIR}"

    "${GRPC_INCLUDE_DIR}"
)


link_grpc(web_server)


# ============================================================
# Proto dependency
# ============================================================

add_dependencies(
    server
    generate_proto
)

add_dependencies(
    client
    generate_proto
)

add_dependencies(
    web_server
    generate_proto
)