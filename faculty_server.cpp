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

using enrollment::FacultyService;
using enrollment::CoreService;

using enrollment::LoginRequest;
using enrollment::LoginReply;

using enrollment::LogoutRequest;
using enrollment::LogoutReply;

using enrollment::UploadGradeRequest;
using enrollment::UploadGradeReply;


// ============================================================
// FACULTY SERVICE
// Proxies faculty requests to the Admin/Core server.
// ============================================================

class FacultyServiceImpl final
    : public FacultyService::Service
{
private:

    std::unique_ptr<CoreService::Stub> core_stub;


public:

    FacultyServiceImpl(
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
            << "[FACULTY SERVER] Login RPC received."
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
                << "[FACULTY SERVER] Core server Login failed: "
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
            << "[FACULTY SERVER] Logout RPC received."
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
                << "[FACULTY SERVER] Core server Logout failed: "
                << status.error_message()
                << std::endl;
        }

        return status;
    }


    // ============================================================
    // UPLOAD GRADE
    // Forward grade upload request to CoreService.
    // ============================================================

    Status UploadGrade(
        ServerContext* context,
        const UploadGradeRequest* request,
        UploadGradeReply* reply
    ) override
    {
        std::cout
            << "[FACULTY SERVER] UploadGrade RPC received."
            << std::endl;

        ClientContext client_context;

        Status status =
            core_stub->UploadGrade(
                &client_context,
                *request,
                reply
            );

        if (!status.ok())
        {
            std::cerr
                << "[FACULTY SERVER] Core server UploadGrade failed: "
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
        << "     FACULTY SERVER"
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
        << "[FACULTY SERVER] Core server: "
        << core_address
        << std::endl;


    std::shared_ptr<Channel> core_channel =
        grpc::CreateChannel(
            core_address,
            grpc::InsecureChannelCredentials()
        );


    // ============================================================
    // CREATE FACULTY SERVICE
    // ============================================================

    FacultyServiceImpl service(
        core_channel
    );


    // ============================================================
    // FACULTY SERVER ADDRESS
    // ============================================================

    std::string server_address =
        make_address(
            "0.0.0.0",
            config.faculty_server_port
        );

    std::cout
        << "[FACULTY SERVER] Listening on: "
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
            << "ERROR: Failed to start Faculty Server."
            << std::endl;

        return 1;
    }


    std::cout
        << "[FACULTY SERVER] Server started successfully."
        << std::endl;

    std::cout
        << "[FACULTY SERVER] Waiting for requests..."
        << std::endl;


    // ============================================================
    // KEEP SERVER RUNNING
    // ============================================================

    server->Wait();

    return 0;
}