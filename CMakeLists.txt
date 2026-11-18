cmake_minimum_required(VERSION 3.20)

project(DistributedEnrollmentSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GRPC_INSTALL_DIR "D:/grpc/install")
set(GRPC_INCLUDE_DIR "${GRPC_INSTALL_DIR}/include")
set(GRPC_LIBRARY_DIR "${GRPC_INSTALL_DIR}/lib")

include_directories(
    "${GRPC_INCLUDE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

find_package(Threads REQUIRED)

# ============================================================
# Generate protobuf / gRPC files
# ============================================================

set(PROTO_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/enrollment.proto"
)

set(PROTO_SRC
    "${CMAKE_CURRENT_BINARY_DIR}/enrollment.pb.cc"
)

set(PROTO_HDR
    "${CMAKE_CURRENT_BINARY_DIR}/enrollment.pb.h"
)

set(GRPC_SRC
    "${CMAKE_CURRENT_BINARY_DIR}/enrollment.grpc.pb.cc"
)

set(GRPC_HDR
    "${CMAKE_CURRENT_BINARY_DIR}/enrollment.grpc.pb.h"
)

add_custom_command(
    OUTPUT
        "${PROTO_SRC}"
        "${PROTO_HDR}"
        "${GRPC_SRC}"
        "${GRPC_HDR}"

    COMMAND
        "${GRPC_INSTALL_DIR}/bin/protoc.exe"

    ARGS
        -I "${CMAKE_CURRENT_SOURCE_DIR}"
        --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"
        --grpc_out="${CMAKE_CURRENT_BINARY_DIR}"
        --plugin=protoc-gen-grpc="${GRPC_INSTALL_DIR}/bin/grpc_cpp_plugin.exe"
        "${PROTO_FILE}"

    DEPENDS
        "${PROTO_FILE}"

    COMMENT "Generating protobuf and gRPC files"
)

add_custom_target(
    generate_proto
    DEPENDS
        "${PROTO_SRC}"
        "${PROTO_HDR}"
        "${GRPC_SRC}"
        "${GRPC_HDR}"
)

# ============================================================
# Generated protobuf / gRPC library
# ============================================================

add_library(
    enrollment_proto
    STATIC
        "${PROTO_SRC}"
        "${GRPC_SRC}"
)

add_dependencies(
    enrollment_proto
    generate_proto
)

target_include_directories(
    enrollment_proto
    PUBLIC
        "${GRPC_INCLUDE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

# ============================================================
# gRPC static libraries
# ============================================================

file(
    GLOB
    GRPC_STATIC_LIBRARIES

    "${GRPC_LIBRARY_DIR}/libabsl_*.a"

    "${GRPC_LIBRARY_DIR}/libupb*.a"

    "${GRPC_LIBRARY_DIR}/libutf8*.a"

    "${GRPC_LIBRARY_DIR}/libre2.a"

    "${GRPC_LIBRARY_DIR}/libcares.a"

    "${GRPC_LIBRARY_DIR}/libzlibstatic.a"

    "${GRPC_LIBRARY_DIR}/libprotobuf.a"

    "${GRPC_LIBRARY_DIR}/libgrpc_unsecure.a"

    "${GRPC_LIBRARY_DIR}/libgrpc++_unsecure.a"

    "${GRPC_LIBRARY_DIR}/libgpr.a"

    "${GRPC_LIBRARY_DIR}/libaddress_sorting.a"
)

# ============================================================
# Common libraries
# ============================================================

set(
    WINDOWS_LIBRARIES
    ws2_32
    advapi32
    iphlpapi
    bcrypt
    crypt32
    dbghelp
)

# ============================================================
# Backend server
# ============================================================

add_executable(
    server
    server.cpp
)

target_include_directories(
    server
    PRIVATE
        "${GRPC_INCLUDE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    server
    PRIVATE
        enrollment_proto

        -Wl,--start-group
        ${GRPC_STATIC_LIBRARIES}
        -Wl,--end-group

        Threads::Threads
        ${WINDOWS_LIBRARIES}
)

# ============================================================
# Test client
# ============================================================

add_executable(
    client
    client.cpp
)

target_include_directories(
    client
    PRIVATE
        "${GRPC_INCLUDE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    client
    PRIVATE
        enrollment_proto

        -Wl,--start-group
        ${GRPC_STATIC_LIBRARIES}
        -Wl,--end-group

        Threads::Threads
        ${WINDOWS_LIBRARIES}
)

# ============================================================
# Web server / MVC View node
# ============================================================

add_executable(
    web_server
    web_server.cpp
)

target_include_directories(
    web_server
    PRIVATE
        "${GRPC_INCLUDE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    web_server
    PRIVATE
        enrollment_proto

        -Wl,--start-group
        ${GRPC_STATIC_LIBRARIES}
        -Wl,--end-group

        Threads::Threads
        ${WINDOWS_LIBRARIES}
)