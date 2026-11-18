#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstring>

#include <windows.h>
#include <bcrypt.h>

#include <grpcpp/grpcpp.h>

#include "enrollment.grpc.pb.h"


#pragma comment(lib, "bcrypt.lib")


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
// JWT configuration
// ============================================================
//
// IMPORTANT:
// This secret is shared by the authentication system.
//
// For the current assignment both nodes ultimately trust the
// backend. The web node carries the JWT and sends it back to
// this backend for validation.
//
// In a production system this should be stored securely and
// not hard-coded in source code.
// ============================================================

const std::string JWT_SECRET =
    "DistributedEnrollmentSystem_JWT_Secret_2026";


const long long JWT_LIFETIME_SECONDS =
    3600;


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
// Active JWT session
// ============================================================

struct SessionData
{
    std::string username;
    std::string role;
    long long expiration;
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
    // JWT JTI -> session information
    //
    // The JWT itself is stateless, but this registry lets us
    // immediately invalidate a JWT when the user logs out.
    // --------------------------------------------------------

    std::unordered_map<
        std::string,
        SessionData
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
    //
    // Used as part of the unique JTI.
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
    // Base64URL encode
    // ========================================================

    std::string base64url_encode(
        const unsigned char* data,
        size_t length
    )
    {
        static const char table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";


        std::string result;

        int value = 0;
        int bits = -6;


        for (
            size_t i = 0;
            i < length;
            i++
        )
        {
            value =
                (value << 8)
                +
                data[i];


            bits += 8;


            while (
                bits >= 0
            )
            {
                result.push_back(
                    table[
                        (value >> bits)
                        &
                        0x3F
                    ]
                );


                bits -= 6;
            }
        }


        if (
            bits > -6
        )
        {
            result.push_back(
                table[
                    (
                        (value << 8)
                        >>
                        (bits + 8)
                    )
                    &
                    0x3F
                ]
            );
        }


        while (
            result.size() % 4
        )
        {
            result.push_back('=');
        }


        // ----------------------------------------------------
        // JWT uses Base64URL rather than normal Base64.
        // ----------------------------------------------------

        for (
            char& character :
            result
        )
        {
            if (
                character == '+'
            )
            {
                character = '-';
            }
            else if (
                character == '/'
            )
            {
                character = '_';
            }
        }


        result.erase(
            std::remove(
                result.begin(),
                result.end(),
                '='
            ),
            result.end()
        );


        return result;
    }


    // ========================================================
    // Base64URL decode
    // ========================================================

    std::string base64url_decode(
        const std::string& input
    )
    {
        std::string value =
            input;


        for (
            char& character :
            value
        )
        {
            if (
                character == '-'
            )
            {
                character = '+';
            }
            else if (
                character == '_'
            )
            {
                character = '/';
            }
        }


        while (
            value.size() % 4
        )
        {
            value.push_back('=');
        }


        static const std::string table =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";


        std::vector<unsigned char> output;


        int value_number = 0;
        int bits = -8;


        for (
            unsigned char character :
            value
        )
        {
            if (
                character == '='
            )
            {
                break;
            }


            size_t position =
                table.find(
                    character
                );


            if (
                position ==
                std::string::npos
            )
            {
                return "";
            }


            value_number =
                (value_number << 6)
                +
                static_cast<int>(
                    position
                );


            bits += 6;


            if (
                bits >= 0
            )
            {
                output.push_back(
                    static_cast<unsigned char>(
                        (
                            value_number
                            >>
                            bits
                        )
                        &
                        0xFF
                    )
                );


                bits -= 8;
            }
        }


        return std::string(
            output.begin(),
            output.end()
        );
    }


    // ========================================================
    // HMAC-SHA256
    // ========================================================

    std::string hmac_sha256(
        const std::string& message,
        const std::string& secret
    )
    {
        BCRYPT_ALG_HANDLE algorithm =
            nullptr;


        BCRYPT_HASH_HANDLE hash =
            nullptr;


        NTSTATUS status =
            BCryptOpenAlgorithmProvider(
                &algorithm,
                BCRYPT_SHA256_ALGORITHM,
                nullptr,
                BCRYPT_ALG_HANDLE_HMAC_FLAG
            );


        if (
            status != 0
        )
        {
            return "";
        }


        DWORD object_length = 0;
        DWORD result_length = 0;


        status =
            BCryptGetProperty(
                algorithm,
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(
                    &object_length
                ),
                sizeof(object_length),
                &result_length,
                0
            );


        if (
            status != 0
        )
        {
            BCryptCloseAlgorithmProvider(
                algorithm,
                0
            );

            return "";
        }


        std::vector<unsigned char> hash_object(
            object_length
        );


        status =
            BCryptCreateHash(
                algorithm,
                &hash,
                hash_object.data(),
                object_length,
                reinterpret_cast<PUCHAR>(
                    const_cast<char*>(
                        secret.data()
                    )
                ),
                static_cast<ULONG>(
                    secret.size()
                ),
                0
            );


        if (
            status != 0
        )
        {
            BCryptCloseAlgorithmProvider(
                algorithm,
                0
            );

            return "";
        }


        status =
            BCryptHashData(
                hash,
                reinterpret_cast<PUCHAR>(
                    const_cast<char*>(
                        message.data()
                    )
                ),
                static_cast<ULONG>(
                    message.size()
                ),
                0
            );


        if (
            status != 0
        )
        {
            BCryptDestroyHash(
                hash
            );

            BCryptCloseAlgorithmProvider(
                algorithm,
                0
            );

            return "";
        }


        unsigned char digest[32];


        status =
            BCryptFinishHash(
                hash,
                digest,
                sizeof(digest),
                0
            );


        BCryptDestroyHash(
            hash
        );


        BCryptCloseAlgorithmProvider(
            algorithm,
            0
        );


        if (
            status != 0
        )
        {
            return "";
        }


        return std::string(
            reinterpret_cast<char*>(
                digest
            ),
            sizeof(digest)
        );
    }


    // ========================================================
    // Generate random bytes
    // ========================================================

    std::string generate_random_id()
    {
        unsigned char random_bytes[16];


        NTSTATUS status =
            BCryptGenRandom(
                nullptr,
                random_bytes,
                sizeof(random_bytes),
                BCRYPT_USE_SYSTEM_PREFERRED_RNG
            );


        if (
            status != 0
        )
        {
            return "";
        }


        return base64url_encode(
            random_bytes,
            sizeof(random_bytes)
        );
    }


    // ========================================================
    // Current Unix timestamp
    // ========================================================

    long long current_time()
    {
        return std::chrono::duration_cast<
            std::chrono::seconds
        >(
            std::chrono::system_clock::now()
            .time_since_epoch()
        ).count();
    }


    // ========================================================
    // Create JWT
    // ========================================================

    std::string create_jwt(
        const std::string& username,
        const std::string& role,
        const std::string& jti,
        long long issued_at,
        long long expiration
    )
    {
        // ----------------------------------------------------
        // JWT header
        // ----------------------------------------------------

        const std::string header =
            R"({"alg":"HS256","typ":"JWT"})";


        // ----------------------------------------------------
        // JWT payload
        // ----------------------------------------------------

        std::string payload =
            "{"
            "\"sub\":\"" +
            username +
            "\","
            "\"role\":\"" +
            role +
            "\","
            "\"jti\":\"" +
            jti +
            "\","
            "\"iat\":" +
            std::to_string(
                issued_at
            ) +
            ","
            "\"exp\":" +
            std::to_string(
                expiration
            ) +
            "}";


        std::string encoded_header =
            base64url_encode(
                reinterpret_cast<
                    const unsigned char*
                >(
                    header.data()
                ),
                header.size()
            );


        std::string encoded_payload =
            base64url_encode(
                reinterpret_cast<
                    const unsigned char*
                >(
                    payload.data()
                ),
                payload.size()
            );


        std::string signing_input =
            encoded_header +
            "." +
            encoded_payload;


        std::string signature =
            hmac_sha256(
                signing_input,
                JWT_SECRET
            );


        if (
            signature.empty()
        )
        {
            return "";
        }


        std::string encoded_signature =
            base64url_encode(
                reinterpret_cast<
                    const unsigned char*
                >(
                    signature.data()
                ),
                signature.size()
            );


        return signing_input +
            "." +
            encoded_signature;
    }


    // ========================================================
    // Constant-time string comparison
    // ========================================================

    bool secure_compare(
        const std::string& first,
        const std::string& second
    )
    {
        if (
            first.size() !=
            second.size()
        )
        {
            return false;
        }


        unsigned char difference = 0;


        for (
            size_t i = 0;
            i < first.size();
            i++
        )
        {
            difference |=
                static_cast<unsigned char>(
                    first[i]
                    ^
                    second[i]
                );
        }


        return difference == 0;
    }


    // ========================================================
    // Extract JWT claim
    // ========================================================

    bool get_json_string(
        const std::string& json,
        const std::string& key,
        std::string& value
    )
    {
        std::string search =
            "\"" +
            key +
            "\":\"";


        size_t position =
            json.find(
                search
            );


        if (
            position ==
            std::string::npos
        )
        {
            return false;
        }


        position +=
            search.length();


        size_t end =
            json.find(
                '"',
                position
            );


        if (
            end ==
            std::string::npos
        )
        {
            return false;
        }


        value =
            json.substr(
                position,
                end - position
            );


        return true;
    }


    // ========================================================
    // Extract JWT numeric claim
    // ========================================================

    bool get_json_number(
        const std::string& json,
        const std::string& key,
        long long& value
    )
    {
        std::string search =
            "\"" +
            key +
            "\":";


        size_t position =
            json.find(
                search
            );


        if (
            position ==
            std::string::npos
        )
        {
            return false;
        }


        position +=
            search.length();


        size_t end =
            position;


        while (
            end < json.size()
            &&
            std::isdigit(
                static_cast<
                    unsigned char
                >(
                    json[end]
                )
            )
        )
        {
            end++;
        }


        if (
            end ==
            position
        )
        {
            return false;
        }


        try
        {
            value =
                std::stoll(
                    json.substr(
                        position,
                        end - position
                    )
                );
        }
        catch (...)
        {
            return false;
        }


        return true;
    }


    // ========================================================
    // Validate JWT
    //
    // Returns username and role when valid.
    // ========================================================

    bool validate_jwt(
        const std::string& token,
        std::string& username,
        std::string& role,
        std::string& jti
    )
    {
        // ----------------------------------------------------
        // JWT must contain exactly three sections.
        // ----------------------------------------------------

        size_t first_dot =
            token.find('.');


        if (
            first_dot ==
            std::string::npos
        )
        {
            return false;
        }


        size_t second_dot =
            token.find(
                '.',
                first_dot + 1
            );


        if (
            second_dot ==
            std::string::npos
        )
        {
            return false;
        }


        if (
            token.find(
                '.',
                second_dot + 1
            )
            !=
            std::string::npos
        )
        {
            return false;
        }


        std::string encoded_header =
            token.substr(
                0,
                first_dot
            );


        std::string encoded_payload =
            token.substr(
                first_dot + 1,
                second_dot - first_dot - 1
            );


        std::string encoded_signature =
            token.substr(
                second_dot + 1
            );


        // ----------------------------------------------------
        // Verify signature.
        // ----------------------------------------------------

        std::string signing_input =
            encoded_header +
            "." +
            encoded_payload;


        std::string expected_signature =
            hmac_sha256(
                signing_input,
                JWT_SECRET
            );


        std::string actual_signature =
            base64url_decode(
                encoded_signature
            );


        if (
            expected_signature.empty()
            ||
            !secure_compare(
                expected_signature,
                actual_signature
            )
        )
        {
            return false;
        }


        // ----------------------------------------------------
        // Verify header.
        // ----------------------------------------------------

        std::string header =
            base64url_decode(
                encoded_header
            );


        if (
            header !=
            R"({"alg":"HS256","typ":"JWT"})"
        )
        {
            return false;
        }


        // ----------------------------------------------------
        // Decode payload.
        // ----------------------------------------------------

        std::string payload =
            base64url_decode(
                encoded_payload
            );


        if (
            payload.empty()
        )
        {
            return false;
        }


        // ----------------------------------------------------
        // Extract claims.
        // ----------------------------------------------------

        if (
            !get_json_string(
                payload,
                "sub",
                username
            )
        )
        {
            return false;
        }


        if (
            !get_json_string(
                payload,
                "role",
                role
            )
        )
        {
            return false;
        }


        if (
            !get_json_string(
                payload,
                "jti",
                jti
            )
        )
        {
            return false;
        }


        long long expiration = 0;


        if (
            !get_json_number(
                payload,
                "exp",
                expiration
            )
        )
        {
            return false;
        }


        long long issued_at = 0;


        if (
            !get_json_number(
                payload,
                "iat",
                issued_at
            )
        )
        {
            return false;
        }


        long long now =
            current_time();


        // ----------------------------------------------------
        // Check expiration.
        // ----------------------------------------------------

        if (
            now >= expiration
        )
        {
            return false;
        }


        // ----------------------------------------------------
        // Check issued-at timestamp.
        // Prevent obviously invalid future tokens.
        // ----------------------------------------------------

        if (
            issued_at > now + 60
        )
        {
            return false;
        }


        return true;
    }


    // ========================================================
    // Get username from valid JWT
    // ========================================================

    bool get_session_username(
        const std::string& token,
        std::string& username
    )
    {
        std::string role;
        std::string jti;


        if (
            !validate_jwt(
                token,
                username,
                role,
                jti
            )
        )
        {
            return false;
        }


        // ----------------------------------------------------
        // JWT signature and expiry are valid.
        //
        // Now check the server-side active session registry.
        // This makes logout immediately invalidate the JWT.
        // ----------------------------------------------------

        auto iterator =
            sessions.find(
                jti
            );


        if (
            iterator ==
            sessions.end()
        )
        {
            return false;
        }


        // ----------------------------------------------------
        // Make sure the registry agrees with the JWT.
        // ----------------------------------------------------

        if (
            iterator->second.username !=
            username
        )
        {
            return false;
        }


        if (
            iterator->second.role !=
            role
        )
        {
            return false;
        }


        if (
            current_time() >=
            iterator->second.expiration
        )
        {
            sessions.erase(
                iterator
            );

            return false;
        }


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


    // ========================================================
    // Remove expired sessions
    // ========================================================

    void cleanup_expired_sessions()
    {
        long long now =
            current_time();


        for (
            auto iterator =
                sessions.begin();
            iterator !=
                sessions.end();
        )
        {
            if (
                iterator->second.expiration <=
                now
            )
            {
                iterator =
                    sessions.erase(
                        iterator
                    );
            }
            else
            {
                ++iterator;
            }
        }
    }


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


        cleanup_expired_sessions();


        std::string username;
        std::string role;


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
            username =
                "admin123";

            role =
                "admin";
        }


        // ----------------------------------------------------
        // Student
        // ----------------------------------------------------

        if (
            username.empty()
        )
        {
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
                    username =
                        student.username;

                    role =
                        "student";

                    break;
                }
            }
        }


        // ----------------------------------------------------
        // Invalid login
        // ----------------------------------------------------

        if (
            username.empty()
        )
        {
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


        // ----------------------------------------------------
        // Create unique JWT session ID.
        // ----------------------------------------------------

        session_counter++;


        std::string random_id =
            generate_random_id();


        if (
            random_id.empty()
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Unable to create secure session."
            );

            return Status::OK;
        }


        std::string jti =
            std::to_string(
                session_counter
            )
            +
            "_"
            +
            random_id;


        long long issued_at =
            current_time();


        long long expiration =
            issued_at +
            JWT_LIFETIME_SECONDS;


        std::string token =
            create_jwt(
                username,
                role,
                jti,
                issued_at,
                expiration
            );


        if (
            token.empty()
        )
        {
            reply->set_success(
                false
            );

            reply->set_message(
                "Unable to create secure session."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Register active session.
        // ----------------------------------------------------

        sessions[jti] =
        {
            username,
            role,
            expiration
        };


        // ----------------------------------------------------
        // Reply.
        // ----------------------------------------------------

        reply->set_success(
            true
        );


        if (
            role ==
            "admin"
        )
        {
            reply->set_message(
                "Admin login successful."
            );
        }
        else
        {
            reply->set_message(
                "Login successful."
            );
        }


        reply->set_session_token(
            token
        );


        reply->set_role(
            role
        );


        if (
            role ==
            "admin"
        )
        {
            std::cout
                << "Admin logged in."
                << std::endl;
        }
        else
        {
            std::cout
                << "Student logged in: "
                << username
                << std::endl;
        }


        std::cout
            << "JWT session created for "
            << username
            << " (expires in "
            << JWT_LIFETIME_SECONDS
            << " seconds)"
            << std::endl;


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


        std::string username;
        std::string role;
        std::string jti;


        // ----------------------------------------------------
        // Validate JWT first.
        // ----------------------------------------------------

        if (
            !validate_jwt(
                request->session_token(),
                username,
                role,
                jti
            )
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


        // ----------------------------------------------------
        // Find active session.
        // ----------------------------------------------------

        auto iterator =
            sessions.find(
                jti
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
                "Session is already logged out."
            );

            return Status::OK;
        }


        std::cout
            << "Logout: "
            << iterator->second.username
            << std::endl;


        // ----------------------------------------------------
        // Revoke JWT immediately.
        // ----------------------------------------------------

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


        cleanup_expired_sessions();


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


        cleanup_expired_sessions();


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


        cleanup_expired_sessions();


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


        cleanup_expired_sessions();


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


    std::cout
        << "JWT authentication enabled."
        << std::endl;


    std::cout
        << "JWT lifetime: "
        << JWT_LIFETIME_SECONDS
        << " seconds."
        << std::endl;


    server->Wait();


    return 0;
}