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
using enrollment::UploadGradeRequest;
using enrollment::UploadGradeReply;
using enrollment::GetGradesRequest;
using enrollment::GetGradesReply;


// ============================================================
// JWT configuration
// ============================================================

const std::string JWT_SECRET =
    "DistributedEnrollmentSystem_JWT_Secret_2026";

const long long JWT_LIFETIME_SECONDS = 3600;


// ============================================================
// Data structures
// ============================================================

struct Student
{
    std::string username;
    std::string password;
};


struct Faculty
{
    std::string username;
    std::string password;
};


struct CourseData
{
    std::string course_code;
    std::string course_name;
    int slots;
};


struct GradeData
{
    std::string course_code;
    std::string course_name;
    double grade;
};


struct SessionData
{
    std::string username;
    std::string role;
    long long expiration;
};


// ============================================================
// Global data
// ============================================================

std::vector<Student> students =
{
    {"student1", "password123"},
    {"student2", "password123"}
};


std::vector<Faculty> faculty =
{
    {"faculty1", "password123"}
};


std::vector<CourseData> courses =
{
    {"CS101", "Introduction to Computing", 30},
    {"CS102", "Data Structures", 25},
    {"CS103", "Database Systems", 20}
};


// username -> session JTI
std::unordered_map<std::string, SessionData> sessions;


// username -> enrolled course codes
std::unordered_map<std::string, std::vector<std::string>> enrollments;


// username -> grades
std::unordered_map<std::string, std::vector<GradeData>> grades;


// Used only for generating unique IDs
long long session_counter = 0;


// Protects all shared application data
std::mutex data_mutex;


// ============================================================
// Base64 URL encoding
// ============================================================

std::string base64url_encode(const std::vector<unsigned char>& input)
{
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_";

    std::string output;

    int val = 0;
    int valb = -6;

    for (unsigned char c : input)
    {
        val = (val << 8) + c;
        valb += 8;

        while (valb >= 0)
        {
            output.push_back(
                table[(val >> valb) & 0x3F]
            );

            valb -= 6;
        }
    }

    if (valb > -6)
    {
        output.push_back(
            table[((val << 8) >> (valb + 8)) & 0x3F]
        );
    }

    return output;
}


std::string base64url_encode(const std::string& input)
{
    std::vector<unsigned char> bytes(
        input.begin(),
        input.end()
    );

    return base64url_encode(bytes);
}


// ============================================================
// Base64 URL decoding
// ============================================================

std::vector<unsigned char> base64url_decode(
    const std::string& input
)
{
    static const std::string table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_";

    std::vector<int> lookup(256, -1);

    for (int i = 0; i < 64; ++i)
    {
        lookup[
            static_cast<unsigned char>(table[i])
        ] = i;
    }

    std::vector<unsigned char> output;

    int val = 0;
    int valb = -8;

    for (unsigned char c : input)
    {
        if (lookup[c] == -1)
        {
            continue;
        }

        val = (val << 6) + lookup[c];
        valb += 6;

        if (valb >= 0)
        {
            output.push_back(
                static_cast<unsigned char>(
                    (val >> valb) & 0xFF
                )
            );

            valb -= 8;
        }
    }

    return output;
}


// ============================================================
// HMAC-SHA256
// ============================================================

std::string hmac_sha256(
    const std::string& key,
    const std::string& data
)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;

    DWORD object_size = 0;
    DWORD data_size = 0;

    NTSTATUS status;


    // --------------------------------------------------------
    // Open SHA256 HMAC algorithm
    // --------------------------------------------------------

    status =
        BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            BCRYPT_ALG_HANDLE_HMAC_FLAG
        );

    if (status < 0)
    {
        return "";
    }


    // --------------------------------------------------------
    // Get hash object size
    // --------------------------------------------------------

    status =
        BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size),
            sizeof(object_size),
            &data_size,
            0
        );

    if (status < 0)
    {
        BCryptCloseAlgorithmProvider(
            algorithm,
            0
        );

        return "";
    }


    std::vector<unsigned char> hash_object(
        object_size
    );


    // --------------------------------------------------------
    // Create hash
    // --------------------------------------------------------

    status =
        BCryptCreateHash(
            algorithm,
            &hash,
            hash_object.data(),
            object_size,
            reinterpret_cast<PUCHAR>(
                const_cast<char*>(key.data())
            ),
            static_cast<ULONG>(key.size()),
            BCRYPT_HASH_REUSABLE_FLAG
        );

    if (status < 0)
    {
        // Some Windows versions may not support the
        // reusable flag in this configuration.
        status =
            BCryptCreateHash(
                algorithm,
                &hash,
                hash_object.data(),
                object_size,
                reinterpret_cast<PUCHAR>(
                    const_cast<char*>(key.data())
                ),
                static_cast<ULONG>(key.size()),
                0
            );
    }

    if (status < 0)
    {
        BCryptCloseAlgorithmProvider(
            algorithm,
            0
        );

        return "";
    }


    // --------------------------------------------------------
    // Hash data
    // --------------------------------------------------------

    status =
        BCryptHashData(
            hash,
            reinterpret_cast<PUCHAR>(
                const_cast<char*>(data.data())
            ),
            static_cast<ULONG>(data.size()),
            0
        );

    if (status < 0)
    {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(
            algorithm,
            0
        );

        return "";
    }


    // --------------------------------------------------------
    // Finish hash
    // --------------------------------------------------------

    unsigned char result[32];

    status =
        BCryptFinishHash(
            hash,
            result,
            sizeof(result),
            0
        );

    BCryptDestroyHash(hash);

    BCryptCloseAlgorithmProvider(
        algorithm,
        0
    );

    if (status < 0)
    {
        return "";
    }


    return base64url_encode(
        std::vector<unsigned char>(
            result,
            result + sizeof(result)
        )
    );
}


// ============================================================
// Random ID
// ============================================================

std::string generate_random_id()
{
    std::lock_guard<std::mutex> lock(data_mutex);

    ++session_counter;

    auto now =
        std::chrono::system_clock::now();

    auto timestamp =
        std::chrono::duration_cast<
            std::chrono::microseconds
        >(
            now.time_since_epoch()
        ).count();

    std::stringstream ss;

    ss
        << timestamp
        << "-"
        << session_counter;

    return ss.str();
}


// ============================================================
// Current Unix timestamp
// ============================================================

long long current_time()
{
    return std::chrono::duration_cast<
        std::chrono::seconds
    >(
        std::chrono::system_clock::now()
        .time_since_epoch()
    ).count();
}


// ============================================================
// Create JWT
// ============================================================

std::string create_jwt(
    const std::string& username,
    const std::string& role
)
{
    std::cout
        << "[JWT] Creating JWT..."
        << std::endl;

    long long issued_at =
        current_time();

    long long expiration =
        issued_at + JWT_LIFETIME_SECONDS;

    std::string jti =
        generate_random_id();


    std::string header =
        R"({"alg":"HS256","typ":"JWT"})";


    std::stringstream payload_stream;

    payload_stream
        << "{"
        << "\"sub\":\""
        << username
        << "\","
        << "\"role\":\""
        << role
        << "\","
        << "\"jti\":\""
        << jti
        << "\","
        << "\"iat\":"
        << issued_at
        << ","
        << "\"exp\":"
        << expiration
        << "}";


    std::string payload =
        payload_stream.str();


    std::string encoded_header =
        base64url_encode(header);

    std::string encoded_payload =
        base64url_encode(payload);


    std::string signing_input =
        encoded_header
        + "."
        + encoded_payload;


    std::string signature =
        hmac_sha256(
            JWT_SECRET,
            signing_input
        );


    if (signature.empty())
    {
        std::cout
            << "[JWT] ERROR: HMAC generation failed."
            << std::endl;

        return "";
    }


    std::string token =
        signing_input
        + "."
        + signature;


    std::cout
        << "[JWT] JWT created."
        << std::endl;

    return token;
}


// ============================================================
// Constant-time string comparison
// ============================================================

bool secure_compare(
    const std::string& a,
    const std::string& b
)
{
    if (a.size() != b.size())
    {
        return false;
    }

    unsigned char result = 0;

    for (size_t i = 0; i < a.size(); ++i)
    {
        result |=
            static_cast<unsigned char>(
                a[i] ^ b[i]
            );
    }

    return result == 0;
}


// ============================================================
// Get JSON string value
// ============================================================

std::string get_json_string(
    const std::string& json,
    const std::string& key
)
{
    std::string search =
        "\"" + key + "\":\"";

    size_t start =
        json.find(search);

    if (start == std::string::npos)
    {
        return "";
    }

    start += search.length();

    size_t end =
        json.find(
            "\"",
            start
        );

    if (end == std::string::npos)
    {
        return "";
    }

    return json.substr(
        start,
        end - start
    );
}


// ============================================================
// Get JSON number value
// ============================================================

long long get_json_number(
    const std::string& json,
    const std::string& key
)
{
    std::string search =
        "\"" + key + "\":";

    size_t start =
        json.find(search);

    if (start == std::string::npos)
    {
        return -1;
    }

    start += search.length();

    size_t end =
        json.find(
            ",",
            start
        );

    if (end == std::string::npos)
    {
        end =
            json.find(
                "}",
                start
            );
    }

    if (end == std::string::npos)
    {
        return -1;
    }

    try
    {
        return std::stoll(
            json.substr(
                start,
                end - start
            )
        );
    }
    catch (...)
    {
        return -1;
    }
}


// ============================================================
// Validate JWT
// ============================================================

bool validate_jwt(
    const std::string& token,
    std::string& username,
    std::string& role,
    std::string& jti
)
{
    size_t first_dot =
        token.find(".");

    if (first_dot == std::string::npos)
    {
        return false;
    }

    size_t second_dot =
        token.find(
            ".",
            first_dot + 1
        );

    if (second_dot == std::string::npos)
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


    std::string signing_input =
        encoded_header
        + "."
        + encoded_payload;


    std::string expected_signature =
        hmac_sha256(
            JWT_SECRET,
            signing_input
        );


    if (expected_signature.empty())
    {
        return false;
    }


    if (!secure_compare(
            expected_signature,
            encoded_signature))
    {
        return false;
    }


    std::vector<unsigned char>
        payload_bytes =
            base64url_decode(
                encoded_payload
            );


    std::string payload(
        payload_bytes.begin(),
        payload_bytes.end()
    );


    username =
        get_json_string(
            payload,
            "sub"
        );

    role =
        get_json_string(
            payload,
            "role"
        );

    jti =
        get_json_string(
            payload,
            "jti"
        );


    long long expiration =
        get_json_number(
            payload,
            "exp"
        );


    if (username.empty()
        || role.empty()
        || jti.empty()
        || expiration < 0)
    {
        return false;
    }


    if (current_time() >= expiration)
    {
        return false;
    }


    return true;
}


// ============================================================
// Get username from session token
// ============================================================

std::string get_session_username(
    const std::string& token
)
{
    std::string username;
    std::string role;
    std::string jti;

    if (!validate_jwt(
            token,
            username,
            role,
            jti))
    {
        return "";
    }


    std::lock_guard<std::mutex> lock(
        data_mutex
    );


    auto it =
        sessions.find(jti);

    if (it == sessions.end())
    {
        return "";
    }


    if (it->second.username != username)
    {
        return "";
    }


    if (it->second.role != role)
    {
        return "";
    }


    if (current_time()
        >= it->second.expiration)
    {
        return "";
    }


    return username;
}


// ============================================================
// Get role from session token
// ============================================================

std::string get_session_role(
    const std::string& token
)
{
    std::string username;
    std::string role;
    std::string jti;

    if (!validate_jwt(
            token,
            username,
            role,
            jti))
    {
        return "";
    }


    std::lock_guard<std::mutex> lock(
        data_mutex
    );


    auto it =
        sessions.find(jti);

    if (it == sessions.end())
    {
        return "";
    }


    if (it->second.username != username)
    {
        return "";
    }


    if (it->second.role != role)
    {
        return "";
    }


    if (current_time()
        >= it->second.expiration)
    {
        return "";
    }


    return role;
}


// ============================================================
// Check admin session
// ============================================================

bool is_admin_session(
    const std::string& token
)
{
    return get_session_role(token)
        == "admin";
}


// ============================================================
// Check faculty session
// ============================================================

bool is_faculty_session(
    const std::string& token
)
{
    return get_session_role(token)
        == "faculty";
}


// ============================================================
// Cleanup expired sessions
// IMPORTANT:
// This function owns its mutex lock.
// Callers must NOT hold data_mutex when calling it.
// ============================================================

void cleanup_expired_sessions()
{
    std::lock_guard<std::mutex> lock(
        data_mutex
    );


    long long now =
        current_time();


    for (auto it = sessions.begin();
         it != sessions.end();)
    {
        if (now >= it->second.expiration)
        {
            it =
                sessions.erase(it);
        }
        else
        {
            ++it;
        }
    }
}


// ============================================================
// Student exists
// ============================================================

bool student_exists(
    const std::string& username
)
{
    for (const auto& student : students)
    {
        if (student.username == username)
        {
            return true;
        }
    }

    return false;
}


// ============================================================
// Faculty exists
// ============================================================

bool faculty_exists(
    const std::string& username
)
{
    for (const auto& person : faculty)
    {
        if (person.username == username)
        {
            return true;
        }
    }

    return false;
}


// ============================================================
// Find course
// ============================================================

CourseData* find_course(
    const std::string& course_code
)
{
    for (auto& course : courses)
    {
        if (course.course_code == course_code)
        {
            return &course;
        }
    }

    return nullptr;
}


// ============================================================
// Enrollment service
// ============================================================

class EnrollmentServiceImpl final
    : public EnrollmentService::Service
{
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
        (void)context;


        std::cout
            << "[SERVER] Login RPC received."
            << std::endl;


        std::cout
            << "[SERVER] Username: "
            << request->username()
            << std::endl;


        // ----------------------------------------------------
        // Clean expired sessions.
        //
        // cleanup_expired_sessions() obtains data_mutex
        // itself. We do NOT hold data_mutex here.
        // ----------------------------------------------------

        std::cout
            << "[SERVER] Calling cleanup_expired_sessions()..."
            << std::endl;

        cleanup_expired_sessions();

        std::cout
            << "[SERVER] cleanup_expired_sessions() finished."
            << std::endl;


        std::string username =
            request->username();

        std::string password =
            request->password();


        if (username.empty()
            || password.empty())
        {
            reply->set_success(false);

            reply->set_message(
                "Username and password are required."
            );

            return Status::OK;
        }


        // ----------------------------------------------------
        // Admin login
        // ----------------------------------------------------

        if (username == "admin123"
            && password == "password123")
        {
            std::cout
                << "[SERVER] Admin account recognized."
                << std::endl;


            std::cout
                << "[SERVER] Creating admin JWT..."
                << std::endl;

            std::string token =
                create_jwt(
                    username,
                    "admin"
                );


            if (token.empty())
            {
                reply->set_success(false);

                reply->set_message(
                    "Failed to create session token."
                );

                return Status::OK;
            }


            // Extract JTI from token
            std::string token_username;
            std::string token_role;
            std::string token_jti;


            if (!validate_jwt(
                    token,
                    token_username,
                    token_role,
                    token_jti))
            {
                reply->set_success(false);

                reply->set_message(
                    "Failed to validate session token."
                );

                return Status::OK;
            }


            long long expiration =
                current_time()
                + JWT_LIFETIME_SECONDS;


            {
                std::lock_guard<std::mutex> lock(
                    data_mutex
                );

                sessions[token_jti] =
                {
                    username,
                    "admin",
                    expiration
                };
            }


            reply->set_success(true);

            reply->set_message(
                "Login successful."
            );

            reply->set_session_token(
                token
            );

            reply->set_role(
                "admin"
            );


            std::cout
                << "[SERVER] Admin login successful."
                << std::endl;


            return Status::OK;
        }


        // ----------------------------------------------------
        // Student login
        // ----------------------------------------------------

        std::cout
            << "[SERVER] Checking student accounts..."
            << std::endl;


        bool student_login = false;


        {
            std::lock_guard<std::mutex> lock(
                data_mutex
            );


            for (const auto& student : students)
            {
                if (student.username == username
                    && student.password == password)
                {
                    student_login = true;
                    break;
                }
            }
        }


        if (student_login)
        {
            std::cout
                << "[SERVER] Student account recognized."
                << std::endl;


            std::cout
                << "[SERVER] Creating student JWT..."
                << std::endl;


            std::string token =
                create_jwt(
                    username,
                    "student"
                );


            if (token.empty())
            {
                reply->set_success(false);

                reply->set_message(
                    "Failed to create session token."
                );

                return Status::OK;
            }


            std::string token_username;
            std::string token_role;
            std::string token_jti;


            std::cout
                << "[SERVER] Validating generated JWT..."
                << std::endl;


            if (!validate_jwt(
                    token,
                    token_username,
                    token_role,
                    token_jti))
            {
                reply->set_success(false);

                reply->set_message(
                    "Failed to validate session token."
                );

                return Status::OK;
            }


            long long expiration =
                current_time()
                + JWT_LIFETIME_SECONDS;


            std::cout
                << "[SERVER] Updating session registry..."
                << std::endl;


            {
                std::lock_guard<std::mutex> lock(
                    data_mutex
                );

                sessions[token_jti] =
                {
                    username,
                    "student",
                    expiration
                };
            }


            std::cout
                << "[SERVER] Session registry updated."
                << std::endl;


            reply->set_success(true);

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
                << "[SERVER] Student login successful."
                << std::endl;


            std::cout
                << "[SERVER] Sending Login reply."
                << std::endl;


            return Status::OK;
        }


        // ----------------------------------------------------
        // Faculty login
        // ----------------------------------------------------

        std::cout
            << "[SERVER] Checking faculty accounts..."
            << std::endl;


        bool faculty_login = false;


        {
            std::lock_guard<std::mutex> lock(
                data_mutex
            );


            for (const auto& person : faculty)
            {
                if (person.username == username
                    && person.password == password)
                {
                    faculty_login = true;
                    break;
                }
            }
        }


        if (faculty_login)
        {
            std::cout
                << "[SERVER] Faculty account recognized."
                << std::endl;


            std::cout
                << "[SERVER] Creating faculty JWT..."
                << std::endl;


            std::string token =
                create_jwt(
                    username,
                    "faculty"
                );


            if (token.empty())
            {
                reply->set_success(false);

                reply->set_message(
                    "Failed to create session token."
                );

                return Status::OK;
            }


            std::string token_username;
            std::string token_role;
            std::string token_jti;


            if (!validate_jwt(
                    token,
                    token_username,
                    token_role,
                    token_jti))
            {
                reply->set_success(false);

                reply->set_message(
                    "Failed to validate session token."
                );

                return Status::OK;
            }


            long long expiration =
                current_time()
                + JWT_LIFETIME_SECONDS;


            {
                std::lock_guard<std::mutex> lock(
                    data_mutex
                );

                sessions[token_jti] =
                {
                    username,
                    "faculty",
                    expiration
                };
            }


            reply->set_success(true);

            reply->set_message(
                "Login successful."
            );

            reply->set_session_token(
                token
            );

            reply->set_role(
                "faculty"
            );


            std::cout
                << "[SERVER] Faculty login successful."
                << std::endl;


            std::cout
                << "[SERVER] Sending Login reply."
                << std::endl;


            return Status::OK;
        }


        // ----------------------------------------------------
        // Invalid credentials
        // ----------------------------------------------------

        std::cout
            << "[SERVER] Invalid login credentials."
            << std::endl;


        reply->set_success(false);

        reply->set_message(
            "Invalid username or password."
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
        (void)context;


        std::string token =
            request->session_token();


        if (token.empty())
        {
            reply->set_success(false);

            reply->set_message(
                "Session token is required."
            );

            return Status::OK;
        }


        std::string username;
        std::string role;
        std::string jti;


        if (!validate_jwt(
                token,
                username,
                role,
                jti))
        {
            reply->set_success(false);

            reply->set_message(
                "Invalid session token."
            );

            return Status::OK;
        }


        {
            std::lock_guard<std::mutex> lock(
                data_mutex
            );


            auto it =
                sessions.find(jti);


            if (it != sessions.end())
            {
                sessions.erase(it);
            }
        }


        reply->set_success(true);

        reply->set_message(
            "Logout successful."
        );


        std::cout
            << "[SERVER] Logout successful for "
            << username
            << std::endl;


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
        (void)context;


        std::string username =
            get_session_username(
                request->session_token()
            );


        if (username.empty())
        {
            reply->set_success(false);

            reply->set_message(
                "Invalid or expired session."
            );

            return Status::OK;
        }


        {
            std::lock_guard<std::mutex> lock(
                data_mutex
            );


            for (const auto& course : courses)
            {
                auto* output_course =
                    reply->add_courses();


                output_course->set_course_code(
                    course.course_code
                );

                output_course->set_course_name(
                    course.course_name
                );

                output_course->set_slots(
                    course.slots
                );
            }
        }


        reply->set_success(true);

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
        (void)context;


        std::string username =
            get_session_username(
                request->session_token()
            );


        if (username.empty())
        {
            reply->set_success(false);

            reply->set_message(
                "Invalid or expired session."
            );

            return Status::OK;
        }


        if (get_session_role(
                request->session_token())
            != "student")
        {
            reply->set_success(false);

            reply->set_message(
                "Only students can enroll."
            );

            return Status::OK;
        }


        std::string course_code =
            request->course_code();


        if (course_code.empty())
        {
            reply->set_success(false);

            reply->set_message(
                "Course code is required."
            );

            return Status::OK;
        }


        std::lock_guard<std::mutex> lock(
            data_mutex
        );


        CourseData* course =
            find_course(course_code);


        if (course == nullptr)
        {
            reply->set_success(false);

            reply->set_message(
                "Course not found."
            );

            return Status::OK;
        }


        if (course->slots <= 0)
        {
            reply->set_success(false);

            reply->set_message(
                "Course is full."
            );

            return Status::OK;
        }


        auto& student_enrollments =
            enrollments[username];


        if (
            std::find(
                student_enrollments.begin(),
                student_enrollments.end(),
                course_code
            )
            != student_enrollments.end()
        )
        {
            reply->set_success(false);

            reply->set_message(
                "You are already enrolled in this course."
            );

            return Status::OK;
        }


        student_enrollments.push_back(
            course_code
        );


        course->slots--;


        reply->set_success(true);

        reply->set_message(
            "Enrollment successful."
        );


        std::cout
            << "[SERVER] "
            << username
            << " enrolled in "
            << course_code
            << std::endl;


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
        (void)context;


        if (!is_admin_session(
                request->session_token()))
        {
            reply->set_success(false);

            reply->set_message(
                "Only administrators can add students."
            );

            return Status::OK;
        }


        std::string username =
            request->username();

        std::string password =
            request->password();


        if (username.empty()
            || password.empty())
        {
            reply->set_success(false);

            reply->set_message(
                "Username and password are required."
            );

            return Status::OK;
        }


        if (username == "admin123")
        {
            reply->set_success(false);

            reply->set_message(
                "This username is reserved."
            );

            return Status::OK;
        }


        std::lock_guard<std::mutex> lock(
            data_mutex
        );


        if (student_exists(username))
        {
            reply->set_success(false);

            reply->set_message(
                "Student already exists."
            );

            return Status::OK;
        }


        if (faculty_exists(username))
        {
            reply->set_success(false);

            reply->set_message(
                "Username is already used by faculty."
            );

            return Status::OK;
        }


        students.push_back(
            {
                username,
                password
            }
        );


        reply->set_success(true);

        reply->set_message(
            "Student added successfully."
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
        (void)context;


        if (!is_admin_session(
                request->session_token()))
        {
            reply->set_success(false);

            reply->set_message(
                "Only administrators can add courses."
            );

            return Status::OK;
        }


        std::string course_code =
            request->course_code();

        std::string course_name =
            request->course_name();

        int slots =
            request->slots();


        if (course_code.empty()
            || course_name.empty())
        {
            reply->set_success(false);

            reply->set_message(
                "Course code and course name are required."
            );

            return Status::OK;
        }


        if (slots <= 0)
        {
            reply->set_success(false);

            reply->set_message(
                "Slots must be greater than zero."
            );

            return Status::OK;
        }


        std::lock_guard<std::mutex> lock(
            data_mutex
        );


        if (find_course(course_code)
            != nullptr)
        {
            reply->set_success(false);

            reply->set_message(
                "Course already exists."
            );

            return Status::OK;
        }


        courses.push_back(
            {
                course_code,
                course_name,
                slots
            }
        );


        reply->set_success(true);

        reply->set_message(
            "Course added successfully."
        );


        return Status::OK;
    }


    // ========================================================
    // Upload grade
    // ========================================================

    Status UploadGrade(
        ServerContext* context,
        const UploadGradeRequest* request,
        UploadGradeReply* reply
    ) override
    {
        (void)context;


        std::string faculty_username =
            get_session_username(
                request->session_token()
            );


        if (faculty_username.empty())
        {
            reply->set_success(false);

            reply->set_message(
                "Invalid or expired session."
            );

            return Status::OK;
        }


        if (get_session_role(
                request->session_token())
            != "faculty")
        {
            reply->set_success(false);

            reply->set_message(
                "Only faculty can upload grades."
            );

            return Status::OK;
        }


        std::string student_username =
            request->student_username();

        std::string course_code =
            request->course_code();

        double grade =
            request->grade();


        if (student_username.empty()
            || course_code.empty())
        {
            reply->set_success(false);

            reply->set_message(
                "Student username and course code are required."
            );

            return Status::OK;
        }


        if (grade < 0.0
            || grade > 100.0)
        {
            reply->set_success(false);

            reply->set_message(
                "Grade must be between 0 and 100."
            );

            return Status::OK;
        }


        std::lock_guard<std::mutex> lock(
            data_mutex
        );


        if (!student_exists(
                student_username))
        {
            reply->set_success(false);

            reply->set_message(
                "Student not found."
            );

            return Status::OK;
        }


        CourseData* course =
            find_course(course_code);


        if (course == nullptr)
        {
            reply->set_success(false);

            reply->set_message(
                "Course not found."
            );

            return Status::OK;
        }


        auto enrollment_it =
            enrollments.find(
                student_username
            );


        if (enrollment_it
            == enrollments.end())
        {
            reply->set_success(false);

            reply->set_message(
                "Student is not enrolled in this course."
            );

            return Status::OK;
        }


        const auto& student_courses =
            enrollment_it->second;


        if (
            std::find(
                student_courses.begin(),
                student_courses.end(),
                course_code
            )
            == student_courses.end()
        )
        {
            reply->set_success(false);

            reply->set_message(
                "Student is not enrolled in this course."
            );

            return Status::OK;
        }


        auto& student_grades =
            grades[student_username];


        bool updated = false;


        for (auto& existing_grade : student_grades)
        {
            if (existing_grade.course_code
                == course_code)
            {
                existing_grade.course_name =
                    course->course_name;

                existing_grade.grade =
                    grade;

                updated = true;

                break;
            }
        }


        if (!updated)
        {
            student_grades.push_back(
                {
                    course_code,
                    course->course_name,
                    grade
                }
            );
        }


        reply->set_success(true);

        reply->set_message(
            updated
                ? "Grade updated successfully."
                : "Grade uploaded successfully."
        );


        std::cout
            << "[SERVER] Faculty "
            << faculty_username
            << " uploaded grade "
            << grade
            << " for "
            << student_username
            << " in "
            << course_code
            << std::endl;


        return Status::OK;
    }


    // ========================================================
    // Get grades
    // ========================================================

    Status GetGrades(
        ServerContext* context,
        const GetGradesRequest* request,
        GetGradesReply* reply
    ) override
    {
        (void)context;


        std::string username =
            get_session_username(
                request->session_token()
            );


        if (username.empty())
        {
            reply->set_success(false);

            reply->set_message(
                "Invalid or expired session."
            );

            return Status::OK;
        }


        if (get_session_role(
                request->session_token())
            != "student")
        {
            reply->set_success(false);

            reply->set_message(
                "Only students can view grades."
            );

            return Status::OK;
        }


        std::lock_guard<std::mutex> lock(
            data_mutex
        );


        auto it =
            grades.find(username);


        if (it != grades.end())
        {
            for (const auto& grade : it->second)
            {
                auto* output_grade =
                    reply->add_grades();


                output_grade->set_course_code(
                    grade.course_code
                );

                output_grade->set_course_name(
                    grade.course_name
                );

                output_grade->set_grade(
                    grade.grade
                );
            }
        }


        reply->set_success(true);

        reply->set_message(
            "Grades retrieved successfully."
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


    grpc::ServerBuilder builder;


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
        std::cerr
            << "ERROR: Failed to start gRPC server."
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


    std::cout
        << "Faculty account: faculty1 / password123"
        << std::endl;


    server->Wait();


    return 0;
}