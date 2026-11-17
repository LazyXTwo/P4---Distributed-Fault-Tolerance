#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "enrollment.grpc.pb.h"
#include "enrollment.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using enrollment::EnrollmentService;
using enrollment::LoginRequest;
using enrollment::LoginReply;
using enrollment::LogoutRequest;
using enrollment::LogoutReply;
using enrollment::GetCoursesRequest;
using enrollment::GetCoursesReply;
using enrollment::EnrollRequest;
using enrollment::EnrollReply;


class EnrollmentClient
{
private:

    std::unique_ptr<EnrollmentService::Stub> stub_;

    std::string session_token;


public:

    EnrollmentClient(
        std::shared_ptr<Channel> channel
    )
        : stub_(EnrollmentService::NewStub(channel))
    {
    }


    // ========================================================
    // Login
    // ========================================================

    bool Login(
        const std::string& username,
        const std::string& password
    )
    {
        LoginRequest request;

        request.set_username(username);
        request.set_password(password);


        LoginReply reply;

        ClientContext context;


        Status status = stub_->Login(
            &context,
            request,
            &reply
        );


        if (!status.ok())
        {
            std::cout
                << "RPC failed: "
                << status.error_message()
                << std::endl;

            return false;
        }


        std::cout
            << reply.message()
            << std::endl;


        if (reply.success())
        {
            session_token = reply.session_token();

            return true;
        }


        return false;
    }


    // ========================================================
    // Get courses
    // ========================================================

    void GetCourses()
    {
        GetCoursesRequest request;

        request.set_session_token(session_token);


        GetCoursesReply reply;

        ClientContext context;


        Status status = stub_->GetCourses(
            &context,
            request,
            &reply
        );


        if (!status.ok())
        {
            std::cout
                << "RPC failed: "
                << status.error_message()
                << std::endl;

            return;
        }


        if (!reply.success())
        {
            std::cout
                << reply.message()
                << std::endl;

            return;
        }


        std::cout << std::endl;
        std::cout << "Available Courses" << std::endl;
        std::cout << "------------------------" << std::endl;


        for (const auto& course : reply.courses())
        {
            std::cout
                << course.course_code()
                << " - "
                << course.course_name()
                << " ("
                << course.slots()
                << " slots)"
                << std::endl;
        }


        std::cout << std::endl;
    }


    // ========================================================
    // Enroll
    // ========================================================

    void Enroll(
        const std::string& course_code
    )
    {
        EnrollRequest request;

        request.set_session_token(session_token);
        request.set_course_code(course_code);


        EnrollReply reply;

        ClientContext context;


        Status status = stub_->Enroll(
            &context,
            request,
            &reply
        );


        if (!status.ok())
        {
            std::cout
                << "RPC failed: "
                << status.error_message()
                << std::endl;

            return;
        }


        std::cout
            << reply.message()
            << std::endl;
    }


    // ========================================================
    // Logout
    // ========================================================

    void Logout()
    {
        LogoutRequest request;

        request.set_session_token(session_token);


        LogoutReply reply;

        ClientContext context;


        Status status = stub_->Logout(
            &context,
            request,
            &reply
        );


        if (!status.ok())
        {
            std::cout
                << "RPC failed: "
                << status.error_message()
                << std::endl;

            return;
        }


        std::cout
            << reply.message()
            << std::endl;


        session_token.clear();
    }
};


// ============================================================
// Main
// ============================================================

int main()
{
    auto channel = grpc::CreateChannel(
        "192.168.100.22:50051",
        grpc::InsecureChannelCredentials()
    );


    EnrollmentClient client(channel);


    std::cout << "================================" << std::endl;
    std::cout << "      ONLINE ENROLLMENT" << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << std::endl;


    std::string username;
    std::string password;


    std::cout << "Username: ";
    std::cin >> username;


    std::cout << "Password: ";
    std::cin >> password;


    std::cout << std::endl;


    if (!client.Login(username, password))
    {
        return 0;
    }


    while (true)
    {
        std::cout << std::endl;

        std::cout << "1. View Courses" << std::endl;
        std::cout << "2. Enroll" << std::endl;
        std::cout << "3. Logout" << std::endl;

        std::cout << std::endl;
        std::cout << "Choice: ";


        int choice;
        std::cin >> choice;


        if (choice == 1)
        {
            client.GetCourses();
        }
        else if (choice == 2)
        {
            std::string course_code;

            std::cout << "Course code: ";
            std::cin >> course_code;

            client.Enroll(course_code);
        }
        else if (choice == 3)
        {
            client.Logout();

            break;
        }
        else
        {
            std::cout << "Invalid choice." << std::endl;
        }
    }


    return 0;
}