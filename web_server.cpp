
#include <iostream>
#include <string>
#include <thread>
#include <sstream>
#include <unordered_map>
#include <mutex>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <grpcpp/grpcpp.h>

#include "enrollment.grpc.pb.h"


#pragma comment(lib, "ws2_32.lib")


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
// gRPC backend
// ============================================================

std::shared_ptr<grpc::Channel> backend_channel;


// ============================================================
// Web sessions
// ============================================================

struct WebSession
{
    std::string grpc_token;
    std::string role;
};


std::unordered_map<
    std::string,
    WebSession
> web_sessions;


std::mutex web_sessions_mutex;


// ============================================================
// Session counter
// ============================================================

unsigned long long session_counter = 0;

std::mutex session_counter_mutex;


// ============================================================
// Create gRPC stub
// ============================================================

std::unique_ptr<EnrollmentService::Stub> create_stub()
{
    return EnrollmentService::NewStub(
        backend_channel
    );
}


// ============================================================
// URL decode
// ============================================================

std::string url_decode(
    const std::string& value
)
{
    std::string result;

    for (
        size_t i = 0;
        i < value.length();
        i++
    )
    {
        if (
            value[i] == '%'
            &&
            i + 2 < value.length()
        )
        {
            int number;

            std::stringstream stream;

            stream
                << std::hex
                << value.substr(
                    i + 1,
                    2
                );

            stream >> number;

            result += static_cast<char>(
                number
            );

            i += 2;
        }
        else if (
            value[i] == '+'
        )
        {
            result += ' ';
        }
        else
        {
            result += value[i];
        }
    }

    return result;
}


// ============================================================
// Get form value
// ============================================================

std::string get_form_value(
    const std::string& body,
    const std::string& key
)
{
    std::string search =
        key + "=";


    size_t position =
        body.find(
            search
        );


    if (
        position ==
        std::string::npos
    )
    {
        return "";
    }


    position +=
        search.length();


    size_t end =
        body.find(
            '&',
            position
        );


    if (
        end ==
        std::string::npos
    )
    {
        return url_decode(
            body.substr(
                position
            )
        );
    }


    return url_decode(
        body.substr(
            position,
            end - position
        )
    );
}


// ============================================================
// Get cookie
// ============================================================

std::string get_cookie(
    const std::string& request,
    const std::string& cookie_name
)
{
    size_t cookie_position =
        request.find(
            "Cookie:"
        );


    if (
        cookie_position ==
        std::string::npos
    )
    {
        return "";
    }


    size_t line_end =
        request.find(
            "\r\n",
            cookie_position
        );


    std::string cookie_line =
        request.substr(
            cookie_position,
            line_end - cookie_position
        );


    std::string search =
        cookie_name + "=";


    size_t value_position =
        cookie_line.find(
            search
        );


    if (
        value_position ==
        std::string::npos
    )
    {
        return "";
    }


    value_position +=
        search.length();


    size_t end =
        cookie_line.find(
            ';',
            value_position
        );


    if (
        end ==
        std::string::npos
    )
    {
        return cookie_line.substr(
            value_position
        );
    }


    return cookie_line.substr(
        value_position,
        end - value_position
    );
}


// ============================================================
// Create web session
// ============================================================

std::string create_web_session(
    const std::string& grpc_token,
    const std::string& role
)
{
    std::lock_guard<std::mutex> lock(
        web_sessions_mutex
    );


    std::lock_guard<std::mutex> counter_lock(
        session_counter_mutex
    );


    session_counter++;


    std::string web_session =
        "web_" +
        std::to_string(
            session_counter
        );


    web_sessions[
        web_session
    ] =
    {
        grpc_token,
        role
    };


    return web_session;
}


// ============================================================
// Get web session
// ============================================================

bool get_web_session(
    const std::string& web_session,
    WebSession& session
)
{
    std::lock_guard<std::mutex> lock(
        web_sessions_mutex
    );


    auto iterator =
        web_sessions.find(
            web_session
        );


    if (
        iterator ==
        web_sessions.end()
    )
    {
        return false;
    }


    session =
        iterator->second;


    return true;
}


// ============================================================
// Remove web session
// ============================================================

void remove_web_session(
    const std::string& web_session
)
{
    std::lock_guard<std::mutex> lock(
        web_sessions_mutex
    );


    web_sessions.erase(
        web_session
    );
}


// ============================================================
// Send HTTP response
// ============================================================

void send_response(
    SOCKET client_socket,
    const std::string& body,
    const std::string& extra_headers = ""
)
{
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " +
        std::to_string(
            body.size()
        ) +
        "\r\n"
        "Connection: close\r\n"
        +
        extra_headers
        +
        "\r\n"
        +
        body;


    send(
        client_socket,
        response.c_str(),
        static_cast<int>(
            response.size()
        ),
        0
    );
}


// ============================================================
// Send redirect
// ============================================================

void send_redirect(
    SOCKET client_socket,
    const std::string& location,
    const std::string& extra_headers = ""
)
{
    std::string response =
        "HTTP/1.1 302 Found\r\n"
        "Location: " +
        location +
        "\r\n"
        "Connection: close\r\n"
        +
        extra_headers
        +
        "\r\n";


    send(
        client_socket,
        response.c_str(),
        static_cast<int>(
            response.size()
        ),
        0
    );
}


// ============================================================
// Login
// ============================================================

void handle_login(
    SOCKET client_socket,
    const std::string& request
)
{
    size_t body_position =
        request.find(
            "\r\n\r\n"
        );


    if (
        body_position ==
        std::string::npos
    )
    {
        closesocket(
            client_socket
        );

        return;
    }


    std::string body =
        request.substr(
            body_position + 4
        );


    std::string username =
        get_form_value(
            body,
            "username"
        );


    std::string password =
        get_form_value(
            body,
            "password"
        );


    std::cout
        << "Login request from: "
        << username
        << std::endl;


    auto stub =
        create_stub();


    LoginRequest login_request;


    login_request.set_username(
        username
    );


    login_request.set_password(
        password
    );


    LoginReply login_reply;


    grpc::ClientContext context;


    grpc::Status status =
        stub->Login(
            &context,
            login_request,
            &login_reply
        );


    if (
        !status.ok()
    )
    {
        std::string html =
            R"(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Online Enrollment</title>
</head>
<body>

<h1>Online Enrollment</h1>

<h2>Backend unavailable</h2>

<p>
The enrollment server could not be reached.
</p>

<a href="/">
Back to login
</a>

</body>
</html>
)";


        send_response(
            client_socket,
            html
        );


        closesocket(
            client_socket
        );


        return;
    }


    if (
        !login_reply.success()
    )
    {
        std::string html =
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<meta charset=\"UTF-8\">"
            "<title>Login Failed</title>"
            "</head>"
            "<body>"
            "<h1>Login failed</h1>"
            "<p>" +
            login_reply.message() +
            "</p>"
            "<a href=\"/\">Try again</a>"
            "</body>"
            "</html>";


        send_response(
            client_socket,
            html
        );


        closesocket(
            client_socket
        );


        return;
    }


    // --------------------------------------------------------
    // Create web session
    // --------------------------------------------------------

    std::string web_session =
        create_web_session(
            login_reply.session_token(),
            login_reply.role()
        );


    std::cout
        << "Login successful: "
        << username
        << " ("
        << login_reply.role()
        << ")"
        << std::endl;


    // --------------------------------------------------------
    // Admin
    // --------------------------------------------------------

    if (
        login_reply.role() ==
        "admin"
    )
    {
        std::string headers =
            "Set-Cookie: session=" +
            web_session +
            "; Path=/\r\n";


        send_redirect(
            client_socket,
            "/admin",
            headers
        );


        closesocket(
            client_socket
        );


        return;
    }


    // --------------------------------------------------------
    // Student
    // --------------------------------------------------------

    std::string headers =
        "Set-Cookie: session=" +
        web_session +
        "; Path=/\r\n";


    send_redirect(
        client_socket,
        "/courses",
        headers
    );


    closesocket(
        client_socket
    );
}


// ============================================================
// Admin page
// ============================================================

void handle_admin(
    SOCKET client_socket,
    const std::string& request
)
{
    std::string web_session =
        get_cookie(
            request,
            "session"
        );


    WebSession session;


    if (
        web_session.empty()
        ||
        !get_web_session(
            web_session,
            session
        )
        ||
        session.role != "admin"
    )
    {
        send_redirect(
            client_socket,
            "/"
        );


        closesocket(
            client_socket
        );


        return;
    }


    std::string html =
        R"(
<!DOCTYPE html>

<html>

<head>

    <meta charset="UTF-8">

    <title>Administrator</title>

</head>

<body>

    <h1>Administrator Panel</h1>

    <hr>

    <h2>Add Student Account</h2>

    <form
        method="POST"
        action="/admin/add-student"
    >

        <label>
            Username:
        </label>

        <input
            type="text"
            name="username"
            required
        >

        <br><br>

        <label>
            Password:
        </label>

        <input
            type="password"
            name="password"
            required
        >

        <br><br>

        <button type="submit">
            Add Student
        </button>

    </form>

    <hr>

    <h2>Add Course</h2>

    <form
        method="POST"
        action="/admin/add-course"
    >

        <label>
            Course Code:
        </label>

        <input
            type="text"
            name="course_code"
            placeholder="CS104"
            required
        >

        <br><br>

        <label>
            Course Name:
        </label>

        <input
            type="text"
            name="course_name"
            placeholder="Computer Networks"
            required
        >

        <br><br>

        <label>
            Number of Slots:
        </label>

        <input
            type="number"
            name="slots"
            min="0"
            required
        >

        <br><br>

        <button type="submit">
            Add Course
        </button>

    </form>

    <hr>

    <a href="/courses">
        View Courses
    </a>

    <br><br>

    <a href="/logout">
        Logout
    </a>

</body>

</html>
)";


    send_response(
        client_socket,
        html
    );


    closesocket(
        client_socket
    );
}


// ============================================================
// Add student
// ============================================================

void handle_add_student(
    SOCKET client_socket,
    const std::string& request
)
{
    std::string web_session =
        get_cookie(
            request,
            "session"
        );


    WebSession session;


    if (
        !get_web_session(
            web_session,
            session
        )
        ||
        session.role != "admin"
    )
    {
        send_redirect(
            client_socket,
            "/"
        );


        closesocket(
            client_socket
        );


        return;
    }


    size_t body_position =
        request.find(
            "\r\n\r\n"
        );


    if (
        body_position ==
        std::string::npos
    )
    {
        closesocket(
            client_socket
        );

        return;
    }


    std::string body =
        request.substr(
            body_position + 4
        );


    std::string username =
        get_form_value(
            body,
            "username"
        );


    std::string password =
        get_form_value(
            body,
            "password"
        );


    auto stub =
        create_stub();


    AddStudentRequest add_request;


    add_request.set_session_token(
        session.grpc_token
    );


    add_request.set_username(
        username
    );


    add_request.set_password(
        password
    );


    AddStudentReply reply;


    grpc::ClientContext context;


    grpc::Status status =
        stub->AddStudent(
            &context,
            add_request,
            &reply
        );


    std::string message;


    if (
        !status.ok()
    )
    {
        message =
            "Backend unavailable.";
    }
    else
    {
        message =
            reply.message();
    }


    std::string html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<title>Add Student</title>"
        "</head>"
        "<body>"
        "<h1>Add Student</h1>"
        "<p>" +
        message +
        "</p>"
        "<a href=\"/admin\">Back to Admin Panel</a>"
        "</body>"
        "</html>";


    send_response(
        client_socket,
        html
    );


    closesocket(
        client_socket
    );
}


// ============================================================
// Add course
// ============================================================

void handle_add_course(
    SOCKET client_socket,
    const std::string& request
)
{
    std::string web_session =
        get_cookie(
            request,
            "session"
        );


    WebSession session;


    if (
        !get_web_session(
            web_session,
            session
        )
        ||
        session.role != "admin"
    )
    {
        send_redirect(
            client_socket,
            "/"
        );


        closesocket(
            client_socket
        );


        return;
    }


    size_t body_position =
        request.find(
            "\r\n\r\n"
        );


    if (
        body_position ==
        std::string::npos
    )
    {
        closesocket(
            client_socket
        );

        return;
    }


    std::string body =
        request.substr(
            body_position + 4
        );


    std::string course_code =
        get_form_value(
            body,
            "course_code"
        );


    std::string course_name =
        get_form_value(
            body,
            "course_name"
        );


    std::string slots_text =
        get_form_value(
            body,
            "slots"
        );


    int slots = 0;


    try
    {
        slots =
            std::stoi(
                slots_text
            );
    }
    catch (...)
    {
        slots = -1;
    }


    auto stub =
        create_stub();


    AddCourseRequest add_request;


    add_request.set_session_token(
        session.grpc_token
    );


    add_request.set_course_code(
        course_code
    );


    add_request.set_course_name(
        course_name
    );


    add_request.set_slots(
        slots
    );


    AddCourseReply reply;


    grpc::ClientContext context;


    grpc::Status status =
        stub->AddCourse(
            &context,
            add_request,
            &reply
        );


    std::string message;


    if (
        !status.ok()
    )
    {
        message =
            "Backend unavailable.";
    }
    else
    {
        message =
            reply.message();
    }


    std::string html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<title>Add Course</title>"
        "</head>"
        "<body>"
        "<h1>Add Course</h1>"
        "<p>" +
        message +
        "</p>"
        "<a href=\"/admin\">Back to Admin Panel</a>"
        "</body>"
        "</html>";


    send_response(
        client_socket,
        html
    );


    closesocket(
        client_socket
    );
}


// ============================================================
// Show courses
// ============================================================

void handle_courses(
    SOCKET client_socket,
    const std::string& request
)
{
    std::string web_session =
        get_cookie(
            request,
            "session"
        );


    WebSession session;


    if (
        web_session.empty()
        ||
        !get_web_session(
            web_session,
            session
        )
    )
    {
        send_redirect(
            client_socket,
            "/"
        );


        closesocket(
            client_socket
        );


        return;
    }


    auto stub =
        create_stub();


    GetCoursesRequest courses_request;


    courses_request.set_session_token(
        session.grpc_token
    );


    GetCoursesReply courses_reply;


    grpc::ClientContext context;


    grpc::Status status =
        stub->GetCourses(
            &context,
            courses_request,
            &courses_reply
        );


    if (
        !status.ok()
    )
    {
        std::string html =
            "<!DOCTYPE html>"
            "<html>"
            "<body>"
            "<h1>Available Courses</h1>"
            "<h2>Backend unavailable</h2>"
            "<p>The enrollment server could not be reached.</p>"
            "</body>"
            "</html>";


        send_response(
            client_socket,
            html
        );


        closesocket(
            client_socket
        );


        return;
    }


    if (
        !courses_reply.success()
    )
    {
        remove_web_session(
            web_session
        );


        send_redirect(
            client_socket,
            "/"
        );


        closesocket(
            client_socket
        );


        return;
    }


    std::string html =
        R"(
<!DOCTYPE html>

<html>

<head>

    <meta charset="UTF-8">

    <title>Available Courses</title>

</head>

<body>

    <h1>Available Courses</h1>

    <hr>
)";


    for (
        const auto& course :
        courses_reply.courses()
    )
    {
        html +=
            "<div>"
            "<h2>" +
            course.course_code() +
            " - " +
            course.course_name() +
            "</h2>"
            "<p>Available slots: " +
            std::to_string(
                course.slots()
            ) +
            "</p>";


        if (
            session.role ==
            "student"
            &&
            course.slots() > 0
        )
        {
            html +=
                "<form method=\"POST\" action=\"/enroll\">"
                "<input type=\"hidden\" name=\"course_code\" value=\"" +
                course.course_code() +
                "\">"
                "<button type=\"submit\">Enroll</button>"
                "</form>";
        }
        else if (
            course.slots() <= 0
        )
        {
            html +=
                "<p><strong>Course Full</strong></p>";
        }


        html +=
            "</div>"
            "<hr>";
    }


    if (
        session.role ==
        "admin"
    )
    {
        html +=
            R"(
    <a href="/admin">
        Administrator Panel
    </a>

    <br><br>
)";
    }


    html +=
        R"(
    <a href="/logout">
        Logout
    </a>

</body>

</html>
)";


    send_response(
        client_socket,
        html
    );


    closesocket(
        client_socket
    );
}


// ============================================================
// Enroll
// ============================================================

void handle_enroll(
    SOCKET client_socket,
    const std::string& request
)
{
    std::string web_session =
        get_cookie(
            request,
            "session"
        );


    WebSession session;


    if (
        !get_web_session(
            web_session,
            session
        )
        ||
        session.role != "student"
    )
    {
        send_redirect(
            client_socket,
            "/"
        );


        closesocket(
            client_socket
        );


        return;
    }


    size_t body_position =
        request.find(
            "\r\n\r\n"
        );


    if (
        body_position ==
        std::string::npos
    )
    {
        closesocket(
            client_socket
        );

        return;
    }


    std::string body =
        request.substr(
            body_position + 4
        );


    std::string course_code =
        get_form_value(
            body,
            "course_code"
        );


    auto stub =
        create_stub();


    EnrollRequest enroll_request;


    enroll_request.set_session_token(
        session.grpc_token
    );


    enroll_request.set_course_code(
        course_code
    );


    EnrollReply enroll_reply;


    grpc::ClientContext context;


    grpc::Status status =
        stub->Enroll(
            &context,
            enroll_request,
            &enroll_reply
        );


    std::string message;


    if (
        !status.ok()
    )
    {
        message =
            "Backend unavailable.";
    }
    else
    {
        message =
            enroll_reply.message();
    }


    std::string html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<title>Enrollment</title>"
        "</head>"
        "<body>"
        "<h1>Enrollment</h1>";


    if (
        status.ok()
        &&
        enroll_reply.success()
    )
    {
        html +=
            "<h2>Enrollment successful</h2>";
    }
    else
    {
        html +=
            "<h2>Enrollment failed</h2>";
    }


    html +=
        "<p>" +
        message +
        "</p>"
        "<a href=\"/courses\">Back to Courses</a>"
        "</body>"
        "</html>";


    send_response(
        client_socket,
        html
    );


    closesocket(
        client_socket
    );
}


// ============================================================
// Logout
// ============================================================

void handle_logout(
    SOCKET client_socket,
    const std::string& request
)
{
    std::string web_session =
        get_cookie(
            request,
            "session"
        );


    WebSession session;


    if (
        get_web_session(
            web_session,
            session
        )
    )
    {
        auto stub =
            create_stub();


        LogoutRequest logout_request;


        logout_request.set_session_token(
            session.grpc_token
        );


        LogoutReply logout_reply;


        grpc::ClientContext context;


        stub->Logout(
            &context,
            logout_request,
            &logout_reply
        );
    }


    remove_web_session(
        web_session
    );


    std::string headers =
        "Set-Cookie: session=; Max-Age=0; Path=/\r\n";


    send_redirect(
        client_socket,
        "/",
        headers
    );


    closesocket(
        client_socket
    );
}


// ============================================================
// Handle browser connection
// ============================================================

void handle_client(
    SOCKET client_socket
)
{
    char buffer[16384];


    int bytes_received =
        recv(
            client_socket,
            buffer,
            sizeof(buffer) - 1,
            0
        );


    if (
        bytes_received <= 0
    )
    {
        closesocket(
            client_socket
        );

        return;
    }


    buffer[
        bytes_received
    ] =
        '\0';


    std::string request(
        buffer
    );


    // ========================================================
    // Login
    // ========================================================

    if (
        request.rfind(
            "POST /login",
            0
        ) == 0
    )
    {
        handle_login(
            client_socket,
            request
        );

        return;
    }


    // ========================================================
    // Admin page
    // ========================================================

    if (
        request.rfind(
            "GET /admin",
            0
        ) == 0
    )
    {
        handle_admin(
            client_socket,
            request
        );

        return;
    }


    // ========================================================
    // Add student
    // ========================================================

    if (
        request.rfind(
            "POST /admin/add-student",
            0
        ) == 0
    )
    {
        handle_add_student(
            client_socket,
            request
        );

        return;
    }


    // ========================================================
    // Add course
    // ========================================================

    if (
        request.rfind(
            "POST /admin/add-course",
            0
        ) == 0
    )
    {
        handle_add_course(
            client_socket,
            request
        );

        return;
    }


    // ========================================================
    // Courses
    // ========================================================

    if (
        request.rfind(
            "GET /courses",
            0
        ) == 0
    )
    {
        handle_courses(
            client_socket,
            request
        );

        return;
    }


    // ========================================================
    // Enroll
    // ========================================================

    if (
        request.rfind(
            "POST /enroll",
            0
        ) == 0
    )
    {
        handle_enroll(
            client_socket,
            request
        );

        return;
    }


    // ========================================================
    // Logout
    // ========================================================

    if (
        request.rfind(
            "GET /logout",
            0
        ) == 0
    )
    {
        handle_logout(
            client_socket,
            request
        );

        return;
    }


    // ========================================================
    // Login page
    // ========================================================

    std::string html =
        R"(
<!DOCTYPE html>

<html>

<head>

    <meta charset="UTF-8">

    <title>Online Enrollment</title>

</head>

<body>

    <h1>Online Enrollment</h1>

    <p>Login to continue.</p>

    <hr>

    <form
        method="POST"
        action="/login"
    >

        <label>
            Username:
        </label>

        <input
            type="text"
            name="username"
            required
        >

        <br><br>

        <label>
            Password:
        </label>

        <input
            type="password"
            name="password"
            required
        >

        <br><br>

        <button type="submit">
            Login
        </button>

    </form>

</body>

</html>
)";


    send_response(
        client_socket,
        html
    );


    closesocket(
        client_socket
    );
}


// ============================================================
// Main
// ============================================================

int main()
{
    // ========================================================
    // Connect to backend
    // ========================================================

    backend_channel =
        grpc::CreateChannel(
            "192.168.100.22:50051",
            grpc::InsecureChannelCredentials()
        );


    std::cout
        << "Connecting to backend:"
        << std::endl;


    std::cout
        << "192.168.100.22:50051"
        << std::endl;


    // ========================================================
    // Initialize Winsock
    // ========================================================

    WSADATA wsa_data;


    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsa_data
        ) != 0
    )
    {
        std::cout
            << "WSAStartup failed."
            << std::endl;

        return 1;
    }


    // ========================================================
    // Create socket
    // ========================================================

    SOCKET server_socket =
        socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        );


    if (
        server_socket ==
        INVALID_SOCKET
    )
    {
        std::cout
            << "Failed to create socket."
            << std::endl;

        WSACleanup();

        return 1;
    }


    // ========================================================
    // Server address
    // ========================================================

    sockaddr_in server_address{};


    server_address.sin_family =
        AF_INET;


    server_address.sin_addr.s_addr =
        INADDR_ANY;


    server_address.sin_port =
        htons(8080);


    // ========================================================
    // Bind
    // ========================================================

    if (
        bind(
            server_socket,
            reinterpret_cast<sockaddr*>(
                &server_address
            ),
            sizeof(server_address)
        )
        == SOCKET_ERROR
    )
    {
        std::cout
            << "Bind failed."
            << std::endl;

        closesocket(
            server_socket
        );

        WSACleanup();

        return 1;
    }


    // ========================================================
    // Listen
    // ========================================================

    if (
        listen(
            server_socket,
            SOMAXCONN
        )
        == SOCKET_ERROR
    )
    {
        std::cout
            << "Listen failed."
            << std::endl;

        closesocket(
            server_socket
        );

        WSACleanup();

        return 1;
    }


    std::cout
        << std::endl;


    std::cout
        << "Web server running at:"
        << std::endl;


    std::cout
        << "http://192.168.100.4:8080"
        << std::endl;


    std::cout
        << std::endl;


    // ========================================================
    // Main request loop
    // ========================================================

    while (true)
    {
        SOCKET client_socket =
            accept(
                server_socket,
                nullptr,
                nullptr
            );


        if (
            client_socket ==
            INVALID_SOCKET
        )
        {
            continue;
        }


        std::thread(
            handle_client,
            client_socket
        ).detach();
    }


    closesocket(
        server_socket
    );


    WSACleanup();


    return 0;
}
