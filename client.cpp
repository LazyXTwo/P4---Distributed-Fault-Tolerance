#include <grpcpp/grpcpp.h>

#include "enrollment.grpc.pb.h"

#include <iostream>
#include <memory>
#include <string>
#include <chrono>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using enrollment::EnrollmentService;
using enrollment::LoginRequest;
using enrollment::LoginReply;


int main()
{
    // ============================================================
    // Connect to backend
    // ============================================================

    std::cout
        << "Creating gRPC channel..."
        << std::endl;

    auto channel =
        grpc::CreateChannel(
            "192.168.100.22:50051",
            grpc::InsecureChannelCredentials()
        );

    std::cout
        << "Creating gRPC stub..."
        << std::endl;

    std::unique_ptr<EnrollmentService::Stub> stub =
        EnrollmentService::NewStub(channel);

    std::cout
        << "gRPC stub created."
        << std::endl;


    // ============================================================
    // Login
    // ============================================================

    std::cout
        << "Preparing Login request..."
        << std::endl;

    LoginRequest request;

    request.set_username("student1");
    request.set_password("password123");

    LoginReply reply;

    ClientContext context;


    // ============================================================
    // Set RPC timeout
    // ============================================================

    context.set_deadline(
        std::chrono::system_clock::now()
        + std::chrono::seconds(10)
    );


    // ============================================================
    // Send Login RPC
    // ============================================================

    std::cout
        << "Sending Login RPC to 192.168.100.22:50051..."
        << std::endl;

    Status status =
        stub->Login(
            &context,
            request,
            &reply
        );


    // ============================================================
    // Display result
    // ============================================================

    std::cout
        << "Login RPC returned."
        << std::endl;

    if (!status.ok())
    {
        std::cout
            << "RPC failed."
            << std::endl;

        std::cout
            << "Error code: "
            << status.error_code()
            << std::endl;

        std::cout
            << "Error message: "
            << status.error_message()
            << std::endl;

        return 1;
    }


    if (reply.success())
    {
        std::cout
            << "Login successful."
            << std::endl;

        std::cout
            << "Session token: "
            << reply.session_token()
            << std::endl;

        std::cout
            << "Role: "
            << reply.role()
            << std::endl;
    }
    else
    {
        std::cout
            << "Login failed: "
            << reply.message()
            << std::endl;
    }


    return 0;
}