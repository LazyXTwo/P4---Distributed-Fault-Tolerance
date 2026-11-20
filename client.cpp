#include <iostream>
#include <memory>

#include <grpcpp/grpcpp.h>

#include "enrollment.grpc.pb.h"
#include "config.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using enrollment::StudentService;
using enrollment::LoginRequest;
using enrollment::LoginReply;


// ============================================================
// MAIN
// ============================================================

int main()
{
    std::cout
        << "========================================"
        << std::endl;

    std::cout
        << "     STUDENT CLIENT TEST"
        << std::endl;

    std::cout
        << "========================================"
        << std::endl;


    // ============================================================
    // LOAD CONFIGURATION
    // ============================================================

    ServerConfig config;

    if (!load_config("config.ini", config))
    {
        std::cerr
            << "ERROR: Could not load config.ini."
            << std::endl;

        return 1;
    }


    // ============================================================
    // CREATE STUDENT SERVER ADDRESS
    // ============================================================

    std::string server_address =
        make_address(
            config.student_server_ip,
            config.student_server_port
        );

    std::cout
        << "Student server: "
        << server_address
        << std::endl;


    // ============================================================
    // CREATE GRPC CHANNEL
    // ============================================================

    std::cout
        << "Creating gRPC channel..."
        << std::endl;

    std::shared_ptr<Channel> channel =
        grpc::CreateChannel(
            server_address,
            grpc::InsecureChannelCredentials()
        );


    // ============================================================
    // CREATE STUDENT SERVICE STUB
    // ============================================================

    std::cout
        << "Creating StudentService stub..."
        << std::endl;

    std::unique_ptr<StudentService::Stub> stub =
        StudentService::NewStub(
            channel
        );

    std::cout
        << "StudentService stub created."
        << std::endl;


    // ============================================================
    // PREPARE LOGIN REQUEST
    // ============================================================

    std::cout
        << "Preparing Login request..."
        << std::endl;

    LoginRequest request;

    request.set_username(
        "student1"
    );

    request.set_password(
        "password123"
    );


    // ============================================================
    // SEND LOGIN REQUEST
    // ============================================================

    std::cout
        << "Sending Login RPC to "
        << server_address
        << "..."
        << std::endl;

    ClientContext context;

    LoginReply reply;

    Status status =
        stub->Login(
            &context,
            request,
            &reply
        );


    // ============================================================
    // CHECK GRPC STATUS
    // ============================================================

    if (!status.ok())
    {
        std::cerr
            << "Login RPC failed."
            << std::endl;

        std::cerr
            << "Error code: "
            << status.error_code()
            << std::endl;

        std::cerr
            << "Error message: "
            << status.error_message()
            << std::endl;

        return 1;
    }


    // ============================================================
    // DISPLAY LOGIN RESULT
    // ============================================================

    std::cout
        << std::endl;

    std::cout
        << "========================================"
        << std::endl;

    std::cout
        << "LOGIN RESULT"
        << std::endl;

    std::cout
        << "========================================"
        << std::endl;

    std::cout
        << "Success: "
        << (reply.success() ? "true" : "false")
        << std::endl;

    std::cout
        << "Message: "
        << reply.message()
        << std::endl;

    std::cout
        << "Role: "
        << reply.role()
        << std::endl;


    if (reply.success())
    {
        std::cout
            << "Session token received: YES"
            << std::endl;

        std::cout
            << "Token length: "
            << reply.session_token().length()
            << std::endl;
    }
    else
    {
        std::cout
            << "Session token received: NO"
            << std::endl;
    }


    std::cout
        << "========================================"
        << std::endl;


    return 0;
}