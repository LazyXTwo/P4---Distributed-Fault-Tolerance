#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <grpcpp/grpcpp.h>

#include "enrollment.grpc.pb.h"
#include "enrollment.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
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
using enrollment::Course;


// ============================================================
// Simple in-memory enrollment data
// ============================================================

struct Student
{
    std::string username;
    std::string password;
};


struct CourseData
{
    std::string code;
    std::string name;
    int slots;
};


// ============================================================
// Enrollment service
// ============================================================

class EnrollmentServiceImpl final : public EnrollmentService::Service
{
private:

    std::vector<Student> students =
    {
        {"student1", "password123"},
        {"student2", "password123"}
    };


    std::vector<CourseData> courses =
    {
        {"CS101", "Introduction to Computing", 30},
        {"CS102", "Data Structures", 25},
        {"CS103", "Database Systems", 20}
    };


    // username -> session token
    std::unordered_map<std::string, std::string> sessions;


    // username -> enrolled course codes
    std::unordered_map<std::string, std::vector<std::string>> enrollments;


    int session_counter = 1;


public:

    // ========================================================
    // Login
    // ========================================================

    Status Login(
        ServerContext* context,
        const LoginRequest* request,
        LoginReply* reply) override
    {
        for (const auto& student : students)
        {
            if (student.username == request->username() &&
                student.password == request->password())
            {
                std::string token =
                    "session_" + std::to_string(session_counter++);

                sessions[student.username] = token;

                reply->set_success(true);
                reply->set_message("Login successful.");
                reply->set_session_token(token);

                std::cout
                    << "Login: "
                    << student.username
                    << std::endl;

                return Status::OK;
            }
        }

        reply->set_success(false);
        reply->set_message("Invalid username or password.");

        std::cout
            << "Failed login: "
            << request->username()
            << std::endl;

        return Status::OK;
    }


    // ========================================================
    // Logout
    // ========================================================

    Status Logout(
        ServerContext* context,
        const LogoutRequest* request,
        LogoutReply* reply) override
    {
        for (auto it = sessions.begin();
             it != sessions.end();
             ++it)
        {
            if (it->second == request->session_token())
            {
                std::cout
                    << "Logout: "
                    << it->first
                    << std::endl;

                sessions.erase(it);

                reply->set_success(true);
                reply->set_message("Logout successful.");

                return Status::OK;
            }
        }

        reply->set_success(false);
        reply->set_message("Invalid session.");

        return Status::OK;
    }


    // ========================================================
    // Get available courses
    // ========================================================

    Status GetCourses(
        ServerContext* context,
        const GetCoursesRequest* request,
        GetCoursesReply* reply) override
    {
        bool valid_session = false;

        for (const auto& session : sessions)
        {
            if (session.second == request->session_token())
            {
                valid_session = true;
                break;
            }
        }

        if (!valid_session)
        {
            reply->set_success(false);
            reply->set_message("Invalid session.");

            return Status::OK;
        }


        for (const auto& course_data : courses)
        {
            Course* course = reply->add_courses();

            course->set_course_code(course_data.code);
            course->set_course_name(course_data.name);
            course->set_slots(course_data.slots);
        }


        reply->set_success(true);
        reply->set_message("Courses retrieved.");

        return Status::OK;
    }


    // ========================================================
    // Enroll
    // ========================================================

    Status Enroll(
        ServerContext* context,
        const EnrollRequest* request,
        EnrollReply* reply) override
    {
        std::string username;


        // ----------------------------------------------------
        // Find the username associated with the session
        // ----------------------------------------------------

        for (const auto& session : sessions)
        {
            if (session.second == request->session_token())
            {
                username = session.first;
                break;
            }
        }


        if (username.empty())
        {
            reply->set_success(false);
            reply->set_message("Invalid session.");

            return Status::OK;
        }


        // ----------------------------------------------------
        // Find the requested course
        // ----------------------------------------------------

        for (auto& course : courses)
        {
            if (course.code == request->course_code())
            {
                if (course.slots <= 0)
                {
                    reply->set_success(false);
                    reply->set_message("Course is full.");

                    return Status::OK;
                }


                // ------------------------------------------------
                // Check if already enrolled
                // ------------------------------------------------

                auto& student_courses =
                    enrollments[username];

                for (const auto& enrolled_course : student_courses)
                {
                    if (enrolled_course == course.code)
                    {
                        reply->set_success(false);
                        reply->set_message(
                            "Already enrolled in this course."
                        );

                        return Status::OK;
                    }
                }


                // ------------------------------------------------
                // Enroll student
                // ------------------------------------------------

                student_courses.push_back(course.code);

                course.slots--;


                reply->set_success(true);
                reply->set_message(
                    "Successfully enrolled in " + course.code + "."
                );


                std::cout
                    << username
                    << " enrolled in "
                    << course.code
                    << std::endl;


                return Status::OK;
            }
        }


        reply->set_success(false);
        reply->set_message("Course not found.");

        return Status::OK;
    }
};


// ============================================================
// Main
// ============================================================

int main()
{
    std::string server_address = "0.0.0.0:50051";

    EnrollmentServiceImpl service;

    ServerBuilder builder;

    builder.AddListeningPort(
        server_address,
        grpc::InsecureServerCredentials()
    );

    builder.RegisterService(&service);

    std::unique_ptr<Server> server(
        builder.BuildAndStart()
    );


    std::cout
        << "Enrollment server listening on "
        << server_address
        << std::endl;


    server->Wait();

    return 0;
}