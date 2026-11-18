
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

#include <grpcpp/grpcpp.h>

#include "enrollment.grpc.pb.h"


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

using enrollment::AddStudentRequest;
using enrollment::AddStudentReply;

using enrollment::AddCourseRequest;
using enrollment::AddCourseReply;


// ============================================================
// Student
// ============================================================

struct Student
{
    std::string username;
    std::string password;
};


// ============================================================
// Course
// ============================================================

struct CourseData
{
    std::string course_code;
    std::string course_name;
    int slots;
};


// ============================================================
// Enrollment server
// ============================================================

class EnrollmentServiceImpl
    final
    : public EnrollmentService::Service
{
private:

    // --------------------------------------------------------
    // Students
    // --------------------------------------------------------

    std::vector<Student> students =
    {
        {
            "student1",
            "password123"
        },

        {
            "student2",
            "password123"
        }
    };


    // --------------------------------------------------------
    // Courses
    // --------------------------------------------------------

    std::vector<CourseData> courses =
    {
        {
            "CS101",
            "Introduction to Computing",
            30
        },

        {
            "CS102",
            "Data Structures",
            25
        },

        {
            "CS103",
            "Database Systems",
            20
        }
    };


    // --------------------------------------------------------
    // Session token -> username
    // --------------------------------------------------------

    std::unordered_map<
        std::string,
        std::string
    > sessions;


    // --------------------------------------------------------
    // Username -> enrolled course codes
    // --------------------------------------------------------

    std::unordered_map<
        std::string,
        std::vector<std::string>
    > enrollments;


    // --------------------------------------------------------
    // Session counter
    // --------------------------------------------------------

    unsigned long long session_counter = 0;


    // --------------------------------------------------------
    // Mutex
    //
    // Protects students, courses, sessions,
    // enrollments and session counter.
    // --------------------------------------------------------

    std::mutex data_mutex;


    // ========================================================
    // Find student
    // ========================================================

    bool student_exists(
        const std::string& username
    )
    {
        for (
            const auto& student :
            students
        )
        {
            if (
                student.username ==
                username
            )
            {
                return true;
            }
        }

        return false;
    }


    // ========================================================
    // Find course
    // ========================================================

    CourseData* find_course(
        const std::string& course_code
    )
    {
        for (
            auto& course :
            courses
        )
        {
            if (
                course.course_code ==
                course_code
            )
            {
                return &course;
            }
        }

        return nullptr;
    }


    // ========================================================
    // Get username from session
    // ========================================================

    bool get_session_username(
        const std::string& token,
        std::string& username
    )
    {
        auto iterator =
            sessions.find(
                token
            );


        if (
            iterator ==
            sessions.end()
        )
        {
            return false;
        }


        username =
            iterator->second;


        return true;
    }


    // ========================================================
    // Check admin session
    // ========================================================

    bool is_admin_session(
        const std::string& token
    )
    {
        std::string username;


        if (
            !get_session_username(
                token,
                username
            )
        )
        {
            return false;
        }


        return username ==
            "admin123";
    }


public:

    // ========================================================
    // Login
    // ========================================================

    Status Login(
        ServerContext* context,
        const LoginRequest* request,
        LoginReply* reply
    ) override
    {
        std::lock_guard<std::mutex> lock(
            data_mutex
        );


        // ----------------------------------------------------
        // Administrator
        // ----------------------------------------------------

        if (
            request->username() ==
                "admin123"
            &&
            request->password() ==
                "password123"
        )
        {
            session_counter++;


            std::string token =
                "session_" +
                std::to_string(
                    session_counter
                );


            sessions[token] =
                "admin123";


            reply->set_success(
                true
            );

            reply->set_message(
                "Admin login successful."
            );

            reply->set_session_token(
                token
            );

            reply->set_role(
                "admin"
            );


            std::cout
                << "Admin logged in."
                << std::endl;


            return Status::OK;
        }


        // ----------------------------------------------------
        // Student
        // ----------------------------------------------------

        for (
            const auto& student :
            students
        )
        {
            if (
                student.username ==
                    request->username()
                &&
                student.password ==
                    request->password()
            )
            {
                session_counter++;


                std::string token =
                    "session_" +
                    std::to_string(
                        session_counter
                    );


                sessions[token] =
                    student.username;


                reply->set_success(
                    true
                );

                reply->set_message(
                    "Login successful."
                );

                reply->set_session_token(
                    token
                );

                reply->set_role(
                    "student"
                );


                std::cout
                    << "Student logged in: "
                    << student.username
                    << std::endl;


                return Status::OK;
            }
        }


        // ----------------------------------------------------
        // Invalid login
        // ----------------------------------------------------

        reply->set_success(
            false
        );

        reply->set_message(
            "Invalid username or password."
        );

        reply->set_role(
            ""
        );


        return Status::OK;
    }


    // ========================================================
    // Logout
    // ========================================================

    Status Logout(
        ServerContext* context,
        const LogoutRequest* request,
        LogoutReply* reply
    ) override
    {
        std::lock_guard<std::mutex> lock(
            data_mutex
        );


        auto iterator =
            sessions.find(
                request->session_token()
            );


        if (
            iterator ==
            sessions.end()
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Invalid session."
            );

            return Status::OK;
        }


        std::cout
            << "Logout: "
            << iterator->second
            << std::endl;


        sessions.erase(
            iterator
        );


        reply->set_success(
            true
        );

        reply->set_message(
            "Logged out successfully."
        );


        return Status::OK;
    }


    // ========================================================
    // Get courses
    // ========================================================

    Status GetCourses(
        ServerContext* context,
        const GetCoursesRequest* request,
        GetCoursesReply* reply
    ) override
    {
        std::lock_guard<std::mutex> lock(
            data_mutex
        );


        std::string username;


        if (
            !get_session_username(
                request->session_token(),
                username
            )
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Invalid or expired session."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Add all courses to response
        // ----------------------------------------------------

        for (
            const auto& course :
            courses
        )
        {
            auto* result_course =
                reply->add_courses();


            result_course->set_course_code(
                course.course_code
            );

            result_course->set_course_name(
                course.course_name
            );

            result_course->set_slots(
                course.slots
            );
        }


        reply->set_success(
            true
        );

        reply->set_message(
            "Courses retrieved successfully."
        );


        return Status::OK;
    }


    // ========================================================
    // Enroll
    // ========================================================

    Status Enroll(
        ServerContext* context,
        const EnrollRequest* request,
        EnrollReply* reply
    ) override
    {
        std::lock_guard<std::mutex> lock(
            data_mutex
        );


        // ----------------------------------------------------
        // Validate session
        // ----------------------------------------------------

        std::string username;


        if (
            !get_session_username(
                request->session_token(),
                username
            )
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Invalid or expired session."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Admin cannot enroll
        // ----------------------------------------------------

        if (
            username ==
            "admin123"
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Administrator accounts cannot enroll."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Find course
        // ----------------------------------------------------

        CourseData* course =
            find_course(
                request->course_code()
            );


        if (
            course ==
            nullptr
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Course not found."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Check duplicate enrollment
        // ----------------------------------------------------

        auto& student_enrollments =
            enrollments[
                username
            ];


        for (
            const auto& enrolled_course :
            student_enrollments
        )
        {
            if (
                enrolled_course ==
                request->course_code()
            )
            {
                reply->set_success(
                    false
                );

                reply->set_message(
                    "You are already enrolled in this course."
                );

                return Status::OK;
            }
        }


        // ----------------------------------------------------
        // Check available slots
        // ----------------------------------------------------

        if (
            course->slots <= 0
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "This course is full."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Enrollment
        //
        // This entire operation is protected by the mutex.
        // Therefore two users cannot take the same final slot.
        // ----------------------------------------------------

        student_enrollments.push_back(
            course->course_code
        );


        course->slots--;


        std::cout
            << username
            << " enrolled in "
            << course->course_code
            << std::endl;


        reply->set_success(
            true
        );

        reply->set_message(
            "Successfully enrolled in " +
            course->course_code +
            "."
        );


        return Status::OK;
    }


    // ========================================================
    // Add student
    // ========================================================

    Status AddStudent(
        ServerContext* context,
        const AddStudentRequest* request,
        AddStudentReply* reply
    ) override
    {
        std::lock_guard<std::mutex> lock(
            data_mutex
        );


        // ----------------------------------------------------
        // Check admin session
        // ----------------------------------------------------

        if (
            !is_admin_session(
                request->session_token()
            )
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Administrator access required."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Validate username
        // ----------------------------------------------------

        if (
            request->username().empty()
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Username cannot be empty."
            );

            return Status::OK;
        }


        if (
            request->password().empty()
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Password cannot be empty."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Check duplicate
        // ----------------------------------------------------

        if (
            student_exists(
                request->username()
            )
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "A student with that username already exists."
            );

            return Status::OK;
        }


        if (
            request->username() ==
            "admin123"
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "That username is reserved."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Add student
        // ----------------------------------------------------

        students.push_back(
            {
                request->username(),
                request->password()
            }
        );


        std::cout
            << "New student added: "
            << request->username()
            << std::endl;


        reply->set_success(
            true
        );

        reply->set_message(
            "Student account created successfully."
        );


        return Status::OK;
    }


    // ========================================================
    // Add course
    // ========================================================

    Status AddCourse(
        ServerContext* context,
        const AddCourseRequest* request,
        AddCourseReply* reply
    ) override
    {
        std::lock_guard<std::mutex> lock(
            data_mutex
        );


        // ----------------------------------------------------
        // Check admin session
        // ----------------------------------------------------

        if (
            !is_admin_session(
                request->session_token()
            )
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Administrator access required."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Validate course
        // ----------------------------------------------------

        if (
            request->course_code().empty()
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Course code cannot be empty."
            );

            return Status::OK;
        }


        if (
            request->course_name().empty()
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Course name cannot be empty."
            );

            return Status::OK;
        }


        if (
            request->slots() < 0
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Number of slots cannot be negative."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Check duplicate course
        // ----------------------------------------------------

        if (
            find_course(
                request->course_code()
            )
            != nullptr
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "A course with that code already exists."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Add course
        // ----------------------------------------------------

        courses.push_back(
            {
                request->course_code(),
                request->course_name(),
                request->slots()
            }
        );


        std::cout
            << "New course added: "
            << request->course_code()
            << " - "
            << request->course_name()
            << " ("
            << request->slots()
            << " slots)"
            << std::endl;


        reply->set_success(
            true
        );

        reply->set_message(
            "Course created successfully."
        );


        return Status::OK;
    }
};


// ============================================================
// Main
// ============================================================

int main()
{
    std::string server_address =
        "0.0.0.0:50051";


    EnrollmentServiceImpl service;


    ServerBuilder builder;


    builder.AddListeningPort(
        server_address,
        grpc::InsecureServerCredentials()
    );


    builder.RegisterService(
        &service
    );


    std::unique_ptr<Server> server =
        builder.BuildAndStart();


    if (!server)
    {
        std::cout
            << "Failed to start server."
            << std::endl;

        return 1;
    }


    std::cout
        << "Enrollment server running on "
        << server_address
        << std::endl;


    server->Wait();


    return 0;
}