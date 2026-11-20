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
# CONFIG FILE
# Copy config.ini into the build directory automatically.
# ============================================================

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.ini"
    "${CMAKE_CURRENT_BINARY_DIR}/config.ini"
    COPYONLY
)


# ============================================================
# PROTO FILE
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


# ============================================================
# GENERATE PROTOBUF / GRPC FILES
# ============================================================

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
# PROTOBUF / GRPC LIBRARY
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
# GRPC STATIC LIBRARIES
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
# WINDOWS LIBRARIES
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
# COMMON SOURCES
# ============================================================

set(
    COMMON_SOURCES
    config.cpp
)


# ============================================================
# SERVER
# Old server kept as backup/development target.
# ============================================================

add_executable(
    server
    server.cpp
    ${COMMON_SOURCES}
)


target_include_directories(
    server
    PRIVATE
        "${GRPC_INCLUDE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
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
# ADMIN SERVER
# Core/shared-state server.
# ============================================================

add_executable(
    admin_server
    admin_server.cpp
    ${COMMON_SOURCES}
)


target_include_directories(
    admin_server
    PRIVATE
        "${GRPC_INCLUDE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)


target_link_libraries(
    admin_server
    PRIVATE
        enrollment_proto

        -Wl,--start-group
        ${GRPC_STATIC_LIBRARIES}
        -Wl,--end-group

        Threads::Threads
        ${WINDOWS_LIBRARIES}
)


# ============================================================
# STUDENT SERVER
# ============================================================

add_executable(
    student_server
    student_server.cpp
    ${COMMON_SOURCES}
)


target_include_directories(
    student_server
    PRIVATE
        "${GRPC_INCLUDE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)


target_link_libraries(
    student_server
    PRIVATE
        enrollment_proto

        -Wl,--start-group
        ${GRPC_STATIC_LIBRARIES}
        -Wl,--end-group

        Threads::Threads
        ${WINDOWS_LIBRARIES}
)

# ============================================================
# FACULTY SERVER
# ============================================================

add_executable(
    faculty_server
    faculty_server.cpp
    ${COMMON_SOURCES}
)


target_include_directories(
    faculty_server
    PRIVATE
        "${GRPC_INCLUDE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)


target_link_libraries(
    faculty_server
    PRIVATE
        enrollment_proto

        -Wl,--start-group
        ${GRPC_STATIC_LIBRARIES}
        -Wl,--end-group

        Threads::Threads
        ${WINDOWS_LIBRARIES}
)

# ============================================================
# CLIENT
# ============================================================

add_executable(
    client
    client.cpp
    ${COMMON_SOURCES}
)


target_include_directories(
    client
    PRIVATE
        "${GRPC_INCLUDE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
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
# WEB SERVER
# ============================================================

add_executable(
    web_server
    web_server.cpp
    ${COMMON_SOURCES}
)


target_include_directories(
    web_server
    PRIVATE
        "${GRPC_INCLUDE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
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