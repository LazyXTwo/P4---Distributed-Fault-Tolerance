#include <iostream>
#include <memory>

#include <grpcpp/grpcpp.h>

#include "enrollment.grpc.pb.h"
#include "config.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using enrollment::StudentService;
using enrollment::CoreService;

using enrollment::LoginRequest;
using enrollment::LoginReply;

using enrollment::LogoutRequest;
using enrollment::LogoutReply;

using enrollment::GetCoursesRequest;
using enrollment::GetCoursesReply;

using enrollment::EnrollRequest;
using enrollment::EnrollReply;

using enrollment::GetGradesRequest;
using enrollment::GetGradesReply;


// ============================================================
// STUDENT SERVICE
// Proxies student requests to the Admin/Core server.
// ============================================================

class StudentServiceImpl final
    : public StudentService::Service
{
private:

    std::unique_ptr<CoreService::Stub> core_stub;


public:

    StudentServiceImpl(
        std::shared_ptr<Channel> core_channel
    )
    {
        core_stub = CoreService::NewStub(
            core_channel
        );
    }


    // ============================================================
    // LOGIN
    // Forward login request to CoreService.
    // ============================================================

    Status Login(
        ServerContext* context,
        const LoginRequest* request,
        LoginReply* reply
    ) override
    {
        std::cout
            << "[STUDENT SERVER] Login RPC received."
            << std::endl;

        ClientContext client_context;

        Status status =
            core_stub->Login(
                &client_context,
                *request,
                reply
            );

        if (!status.ok())
        {
            std::cerr
                << "[STUDENT SERVER] Core server Login failed: "
                << status.error_message()
                << std::endl;
        }

        return status;
    }


    // ============================================================
    // LOGOUT
    // Forward logout request to CoreService.
    // ============================================================

    Status Logout(
        ServerContext* context,
        const LogoutRequest* request,
        LogoutReply* reply
    ) override
    {
        std::cout
            << "[STUDENT SERVER] Logout RPC received."
            << std::endl;

        ClientContext client_context;

        Status status =
            core_stub->Logout(
                &client_context,
                *request,
                reply
            );

        if (!status.ok())
        {
            std::cerr
                << "[STUDENT SERVER] Core server Logout failed: "
                << status.error_message()
                << std::endl;
        }

        return status;
    }


    // ============================================================
    // GET COURSES
    // Forward course request to CoreService.
    // ============================================================

    Status GetCourses(
        ServerContext* context,
        const GetCoursesRequest* request,
        GetCoursesReply* reply
    ) override
    {
        std::cout
            << "[STUDENT SERVER] GetCourses RPC received."
            << std::endl;

        ClientContext client_context;

        Status status =
            core_stub->GetCourses(
                &client_context,
                *request,
                reply
            );

        if (!status.ok())
        {
            std::cerr
                << "[STUDENT SERVER] Core server GetCourses failed: "
                << status.error_message()
                << std::endl;
        }

        return status;
    }


    // ============================================================
    // ENROLL
    // Forward enrollment request to CoreService.
    // ============================================================

    Status Enroll(
        ServerContext* context,
        const EnrollRequest* request,
        EnrollReply* reply
    ) override
    {
        std::cout
            << "[STUDENT SERVER] Enroll RPC received."
            << std::endl;

        ClientContext client_context;

        Status status =
            core_stub->Enroll(
                &client_context,
                *request,
                reply
            );

        if (!status.ok())
        {
            std::cerr
                << "[STUDENT SERVER] Core server Enroll failed: "
                << status.error_message()
                << std::endl;
        }

        return status;
    }


    // ============================================================
    // GET GRADES
    // Forward grades request to CoreService.
    // ============================================================

    Status GetGrades(
        ServerContext* context,
        const GetGradesRequest* request,
        GetGradesReply* reply
    ) override
    {
        std::cout
            << "[STUDENT SERVER] GetGrades RPC received."
            << std::endl;

        ClientContext client_context;

        Status status =
            core_stub->GetGrades(
                &client_context,
                *request,
                reply
            );

        if (!status.ok())
        {
            std::cerr
                << "[STUDENT SERVER] Core server GetGrades failed: "
                << status.error_message()
                << std::endl;
        }

        return status;
    }
};


// ============================================================
// MAIN
// ============================================================

int main()
{
    std::cout
        << "========================================"
        << std::endl;

    std::cout
        << "     STUDENT SERVER"
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
    // CONNECT TO ADMIN/CORE SERVER
    // ============================================================

    std::string core_address =
        make_address(
            config.admin_server_ip,
            config.admin_server_port
        );

    std::cout
        << "[STUDENT SERVER] Core server: "
        << core_address
        << std::endl;


    std::shared_ptr<Channel> core_channel =
        grpc::CreateChannel(
            core_address,
            grpc::InsecureChannelCredentials()
        );


    // ============================================================
    // CREATE STUDENT SERVICE
    // ============================================================

    StudentServiceImpl service(
        core_channel
    );


    // ============================================================
    // STUDENT SERVER ADDRESS
    // ============================================================

    std::string server_address =
        make_address(
            "0.0.0.0",
            config.student_server_port
        );

    std::cout
        << "[STUDENT SERVER] Listening on: "
        << server_address
        << std::endl;


    // ============================================================
    // START GRPC SERVER
    // ============================================================

    ServerBuilder builder;

    builder.AddListeningPort(
        server_address,
        grpc::InsecureServerCredentials()
    );

    builder.RegisterService(
        &service
    );

    std::unique_ptr<Server> server(
        builder.BuildAndStart()
    );


    if (!server)
    {
        std::cerr
            << "ERROR: Failed to start Student Server."
            << std::endl;

        return 1;
    }


    std::cout
        << "[STUDENT SERVER] Server started successfully."
        << std::endl;

    std::cout
        << "[STUDENT SERVER] Waiting for requests..."
        << std::endl;


    // ============================================================
    // KEEP SERVER RUNNING
    // ============================================================

    server->Wait();

    return 0;
}