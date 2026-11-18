#include <grpcpp/grpcpp.h>

#include "enrollment.grpc.pb.h"

#include <iostream>
#include <memory>
#include <string>

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

    auto channel =
        grpc::CreateChannel(
            "192.168.100.22:50051",
            grpc::InsecureChannelCredentials()
        );

    std::unique_ptr<EnrollmentService::Stub> stub =
        EnrollmentService::NewStub(channel);


    // ============================================================
    // Login
    // ============================================================

    LoginRequest request;

    request.set_username("student1");
    request.set_password("password123");

    LoginReply reply;

    ClientContext context;

    Status status =
        stub->Login(
            &context,
            request,
            &reply
        );


    // ============================================================
    // Display result
    // ============================================================

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