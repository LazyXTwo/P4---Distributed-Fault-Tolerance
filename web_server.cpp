#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <memory>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <grpcpp/grpcpp.h>

#include "enrollment.grpc.pb.h"
#include "config.h"


#pragma comment(lib, "ws2_32.lib")


using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;


// ============================================================
// Web session
// ============================================================

struct WebSession {
    std::string grpc_token;
    std::string role;
};


// ============================================================
// Web session storage
// ============================================================

std::unordered_map<std::string, WebSession> web_sessions;

std::mutex web_sessions_mutex;

long long web_session_counter = 0;

std::mutex web_session_counter_mutex;


// ============================================================
// Server configuration
// ============================================================

ServerConfig server_config;


// ============================================================
// Create AdminService stub
// ============================================================

std::unique_ptr<enrollment::AdminService::Stub>
create_admin_stub()
{
    return enrollment::AdminService::NewStub(
        grpc::CreateChannel(
            make_address(
                server_config.admin_server_ip,
                server_config.admin_server_port
            ),
            grpc::InsecureChannelCredentials()
        )
    );
}


// ============================================================
// Create StudentService stub
// ============================================================

std::unique_ptr<enrollment::StudentService::Stub>
create_student_stub()
{
    return enrollment::StudentService::NewStub(
        grpc::CreateChannel(
            make_address(
                server_config.student_server_ip,
                server_config.student_server_port
            ),
            grpc::InsecureChannelCredentials()
        )
    );
}


// ============================================================
// Create FacultyService stub
// ============================================================

std::unique_ptr<enrollment::FacultyService::Stub>
create_faculty_stub()
{
    return enrollment::FacultyService::NewStub(
        grpc::CreateChannel(
            make_address(
                server_config.faculty_server_ip,
                server_config.faculty_server_port
            ),
            grpc::InsecureChannelCredentials()
        )
    );
}


// ============================================================
// URL decode
// ============================================================

std::string url_decode(const std::string& input)
{
    std::string output;

    for (size_t i = 0; i < input.length(); ++i) {

        if (input[i] == '+') {
            output += ' ';
        }

        else if (
            input[i] == '%' &&
            i + 2 < input.length()
        ) {

            std::string hex =
                input.substr(i + 1, 2);

            char character =
                static_cast<char>(
                    std::stoi(
                        hex,
                        nullptr,
                        16
                    )
                );

            output += character;

            i += 2;
        }

        else {
            output += input[i];
        }
    }

    return output;
}


// ============================================================
// Parse form data
// ============================================================

std::unordered_map<std::string, std::string>
parse_form(const std::string& body)
{
    std::unordered_map<std::string, std::string> result;

    std::stringstream stream(body);

    std::string pair;

    while (
        std::getline(
            stream,
            pair,
            '&'
        )
    ) {

        size_t equals =
            pair.find('=');

        if (
            equals ==
            std::string::npos
        ) {
            continue;
        }

        std::string key =
            pair.substr(
                0,
                equals
            );

        std::string value =
            pair.substr(
                equals + 1
            );

        result[
            url_decode(key)
        ] =
            url_decode(value);
    }

    return result;
}


// ============================================================
// Get cookie value
// ============================================================

std::string get_cookie(
    const std::string& request,
    const std::string& cookie_name)
{
    size_t cookie_position =
        request.find("Cookie:");

    if (
        cookie_position ==
        std::string::npos
    ) {
        return "";
    }

    size_t line_end =
        request.find(
            "\r\n",
            cookie_position
        );

    if (
        line_end ==
        std::string::npos
    ) {
        return "";
    }

    std::string cookie_line =
        request.substr(
            cookie_position,
            line_end - cookie_position
        );

    std::string search =
        cookie_name + "=";

    size_t value_start =
        cookie_line.find(search);

    if (
        value_start ==
        std::string::npos
    ) {
        return "";
    }

    value_start += search.length();

    size_t value_end =
        cookie_line.find(
            ';',
            value_start
        );

    if (
        value_end ==
        std::string::npos
    ) {
        value_end =
            cookie_line.length();
    }

    return cookie_line.substr(
        value_start,
        value_end - value_start
    );
}


// ============================================================
// Create web session
// ============================================================

std::string create_web_session(
    const std::string& grpc_token,
    const std::string& role)
{
    std::lock_guard<std::mutex> counter_lock(
        web_session_counter_mutex
    );

    ++web_session_counter;

    std::string session_id =
        "web_" +
        std::to_string(
            web_session_counter
        );

    {
        std::lock_guard<std::mutex> lock(
            web_sessions_mutex
        );

        web_sessions[session_id] =
            WebSession{
                grpc_token,
                role
            };
    }

    return session_id;
}


// ============================================================
// Get web session
// ============================================================

bool get_web_session(
    const std::string& session_id,
    WebSession& session)
{
    if (
        session_id.empty()
    ) {
        return false;
    }

    std::lock_guard<std::mutex> lock(
        web_sessions_mutex
    );

    auto it =
        web_sessions.find(
            session_id
        );

    if (
        it ==
        web_sessions.end()
    ) {
        return false;
    }

    session =
        it->second;

    return true;
}


// ============================================================
// Remove web session
// ============================================================

void remove_web_session(
    const std::string& session_id)
{
    if (
        session_id.empty()
    ) {
        return;
    }

    std::lock_guard<std::mutex> lock(
        web_sessions_mutex
    );

    web_sessions.erase(
        session_id
    );
}


// ============================================================
// Send HTTP response
// ============================================================

void send_response(
    SOCKET client_socket,
    const std::string& body,
    const std::string& status = "200 OK",
    const std::string& extra_headers = "")
{
    std::string response =
        "HTTP/1.1 " +
        status +
        "\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " +
        std::to_string(
            body.size()
        ) +
        "\r\n" +
        extra_headers +
        "Connection: close\r\n"
        "\r\n" +
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
    const std::string& location)
{
    std::string response =
        "HTTP/1.1 302 Found\r\n"
        "Location: " +
        location +
        "\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
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
// HTML header
// ============================================================

std::string html_header(
    const std::string& title)
{
    std::stringstream html;

    html
        << "<!DOCTYPE html>"
        << "<html>"
        << "<head>"
        << "<meta charset='UTF-8'>"
        << "<meta name='viewport' "
           "content='width=device-width, initial-scale=1.0'>"
        << "<title>"
        << title
        << "</title>"

        << "<style>"

        << "body {"
        << "font-family: Arial, sans-serif;"
        << "background:#f4f6f8;"
        << "margin:0;"
        << "padding:0;"
        << "color:#222;"
        << "}"

        << ".container {"
        << "max-width:900px;"
        << "margin:40px auto;"
        << "background:white;"
        << "padding:30px;"
        << "border-radius:12px;"
        << "box-shadow:0 4px 15px rgba(0,0,0,0.08);"
        << "}"

        << "h1 {"
        << "margin-top:0;"
        << "}"

        << "h2 {"
        << "margin-top:30px;"
        << "}"

        << "input, select {"
        << "width:100%;"
        << "padding:12px;"
        << "margin:8px 0 16px 0;"
        << "box-sizing:border-box;"
        << "border:1px solid #ccc;"
        << "border-radius:6px;"
        << "}"

        << "button {"
        << "background:#2563eb;"
        << "color:white;"
        << "border:none;"
        << "padding:11px 18px;"
        << "border-radius:6px;"
        << "cursor:pointer;"
        << "}"

        << "button:hover {"
        << "background:#1d4ed8;"
        << "}"

        << ".danger {"
        << "background:#dc2626;"
        << "}"

        << ".danger:hover {"
        << "background:#b91c1c;"
        << "}"

        << ".success {"
        << "background:#dcfce7;"
        << "color:#166534;"
        << "padding:12px;"
        << "border-radius:6px;"
        << "margin:15px 0;"
        << "}"

        << ".error {"
        << "background:#fee2e2;"
        << "color:#991b1b;"
        << "padding:12px;"
        << "border-radius:6px;"
        << "margin:15px 0;"
        << "}"

        << ".info {"
        << "background:#dbeafe;"
        << "color:#1e40af;"
        << "padding:12px;"
        << "border-radius:6px;"
        << "margin:15px 0;"
        << "}"

        << ".course {"
        << "border:1px solid #ddd;"
        << "padding:18px;"
        << "margin:12px 0;"
        << "border-radius:8px;"
        << "}"

        << ".grade {"
        << "border:1px solid #ddd;"
        << "padding:18px;"
        << "margin:12px 0;"
        << "border-radius:8px;"
        << "display:flex;"
        << "justify-content:space-between;"
        << "align-items:center;"
        << "}"

        << ".nav {"
        << "display:flex;"
        << "gap:10px;"
        << "flex-wrap:wrap;"
        << "margin-bottom:25px;"
        << "}"

        << ".nav a {"
        << "text-decoration:none;"
        << "background:#e5e7eb;"
        << "color:#111827;"
        << "padding:10px 15px;"
        << "border-radius:6px;"
        << "}"

        << ".nav a:hover {"
        << "background:#d1d5db;"
        << "}"

        << "table {"
        << "width:100%;"
        << "border-collapse:collapse;"
        << "margin-top:15px;"
        << "}"

        << "th, td {"
        << "border:1px solid #ddd;"
        << "padding:12px;"
        << "text-align:left;"
        << "}"

        << "th {"
        << "background:#f3f4f6;"
        << "}"

        << "</style>"

        << "</head>"
        << "<body>";

    return html.str();
}


// ============================================================
// HTML footer
// ============================================================

std::string html_footer()
{
    return
        "</body>"
        "</html>";
}


// ============================================================
// Login page
// ============================================================

void handle_login_page(
    SOCKET client_socket,
    const std::string& message = "",
    bool error = false)
{
    std::stringstream html;

    html
        << html_header(
            "Distributed Enrollment System"
        )

        << "<div class='container'>"

        << "<h1>Distributed Enrollment System</h1>"

        << "<p>"
        << "Login to continue."
        << "</p>";

    if (!message.empty()) {

        html
            << "<div class='"
            << (error ? "error" : "success")
            << "'>"
            << message
            << "</div>";
    }

    html
        << "<form method='POST' action='/login'>"

        << "<label>Username</label>"
        << "<input "
           "type='text' "
           "name='username' "
           "required>"

        << "<label>Password</label>"
        << "<input "
           "type='password' "
           "name='password' "
           "required>"

        << "<button type='submit'>"
        << "Login"
        << "</button>"

        << "</form>"

        << "</div>"

        << html_footer();

    send_response(
        client_socket,
        html.str()
    );
}


// ============================================================
// Handle login
// ============================================================

void handle_login(
    SOCKET client_socket,
    const std::string& body)
{
    auto form =
        parse_form(body);

    std::string username =
        form["username"];

    std::string password =
        form["password"];


    // ========================================================
    // Login starts through Admin/Core
    // ========================================================

    auto admin_stub =
        create_admin_stub();

    enrollment::LoginRequest request;

    request.set_username(
        username
    );

    request.set_password(
        password
    );

    enrollment::LoginReply reply;

    ClientContext context;

    Status status =
        admin_stub->Login(
            &context,
            request,
            &reply
        );

    if (
        !status.ok()
    ) {

        handle_login_page(
            client_socket,
            "Could not connect to enrollment server.",
            true
        );

        return;
    }

    if (
        !reply.success()
    ) {

        handle_login_page(
            client_socket,
            reply.message(),
            true
        );

        return;
    }


    // ========================================================
    // Create web session
    // ========================================================

    std::string web_session =
        create_web_session(
            reply.session_token(),
            reply.role()
        );

    std::string location;

    if (
        reply.role() ==
        "admin"
    ) {

        location =
            "/admin";
    }

    else if (
        reply.role() ==
        "faculty"
    ) {

        location =
            "/faculty";
    }

    else {

        location =
            "/courses";
    }


    // ========================================================
    // Redirect with cookie
    // ========================================================

    {
        std::string headers =
            "Set-Cookie: session=" +
            web_session +
            "; HttpOnly; SameSite=Lax\r\n";

        std::string response =
            "HTTP/1.1 302 Found\r\n"
            "Location: " +
            location +
            "\r\n" +
            headers +
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
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
}


// ============================================================
// Admin page
// ============================================================

void handle_admin(
    SOCKET client_socket,
    const WebSession& session,
    const std::string& message = "",
    bool error = false)
{
    if (
        session.role !=
        "admin"
    ) {

        send_redirect(
            client_socket,
            "/courses"
        );

        return;
    }

    std::stringstream html;

    html
        << html_header(
            "Admin Dashboard"
        )

        << "<div class='container'>"

        << "<h1>Admin Dashboard</h1>"

        << "<div class='nav'>"
        << "<a href='/admin'>Admin</a>"
        << "<a href='/courses'>Courses</a>"
        << "<a href='/logout'>Logout</a>"
        << "</div>";

    if (!message.empty()) {

        html
            << "<div class='"
            << (error ? "error" : "success")
            << "'>"
            << message
            << "</div>";
    }


    // ========================================================
    // Add student
    // ========================================================

    html
        << "<h2>Add Student</h2>"

        << "<form method='POST' "
           "action='/admin/add-student'>"

        << "<label>Username</label>"
        << "<input "
           "type='text' "
           "name='username' "
           "required>"

        << "<label>Password</label>"
        << "<input "
           "type='password' "
           "name='password' "
           "required>"

        << "<button type='submit'>"
        << "Add Student"
        << "</button>"

        << "</form>";


    // ========================================================
    // Add course
    // ========================================================

    html
        << "<h2>Add Course</h2>"

        << "<form method='POST' "
           "action='/admin/add-course'>"

        << "<label>Course Code</label>"
        << "<input "
           "type='text' "
           "name='course_code' "
           "required>"

        << "<label>Course Name</label>"
        << "<input "
           "type='text' "
           "name='course_name' "
           "required>"

        << "<label>Slots</label>"
        << "<input "
           "type='number' "
           "name='slots' "
           "min='0' "
           "required>"

        << "<button type='submit'>"
        << "Add Course"
        << "</button>"

        << "</form>"

        << "</div>"

        << html_footer();

    send_response(
        client_socket,
        html.str()
    );
}


// ============================================================
// Add student
// ============================================================

void handle_add_student(
    SOCKET client_socket,
    const WebSession& session,
    const std::string& body)
{
    if (
        session.role !=
        "admin"
    ) {

        send_redirect(
            client_socket,
            "/courses"
        );

        return;
    }

    auto form =
        parse_form(body);

    auto stub =
        create_admin_stub();

    enrollment::AddStudentRequest request;

    request.set_session_token(
        session.grpc_token
    );

    request.set_username(
        form["username"]
    );

    request.set_password(
        form["password"]
    );

    enrollment::AddStudentReply reply;

    ClientContext context;

    Status status =
        stub->AddStudent(
            &context,
            request,
            &reply
        );

    if (
        !status.ok()
    ) {

        handle_admin(
            client_socket,
            session,
            "Could not connect to enrollment server.",
            true
        );

        return;
    }

    handle_admin(
        client_socket,
        session,
        reply.message(),
        !reply.success()
    );
}


// ============================================================
// Add course
// ============================================================

void handle_add_course(
    SOCKET client_socket,
    const WebSession& session,
    const std::string& body)
{
    if (
        session.role !=
        "admin"
    ) {

        send_redirect(
            client_socket,
            "/courses"
        );

        return;
    }

    auto form =
        parse_form(body);

    int slots = 0;

    try {

        slots =
            std::stoi(
                form["slots"]
            );
    }

    catch (...) {

        slots = -1;
    }

    auto stub =
        create_admin_stub();

    enrollment::AddCourseRequest request;

    request.set_session_token(
        session.grpc_token
    );

    request.set_course_code(
        form["course_code"]
    );

    request.set_course_name(
        form["course_name"]
    );

    request.set_slots(
        slots
    );

    enrollment::AddCourseReply reply;

    ClientContext context;

    Status status =
        stub->AddCourse(
            &context,
            request,
            &reply
        );

    if (
        !status.ok()
    ) {

        handle_admin(
            client_socket,
            session,
            "Could not connect to enrollment server.",
            true
        );

        return;
    }

    handle_admin(
        client_socket,
        session,
        reply.message(),
        !reply.success()
    );
}


// ============================================================
// Courses page
// ============================================================

void handle_courses(
    SOCKET client_socket,
    const WebSession& session)
{
    auto stub =
        create_student_stub();

    enrollment::GetCoursesRequest request;

    request.set_session_token(
        session.grpc_token
    );

    enrollment::GetCoursesReply reply;

    ClientContext context;

    Status status =
        stub->GetCourses(
            &context,
            request,
            &reply
        );

    if (
        !status.ok()
    ) {

        send_response(
            client_socket,
            html_header(
                "Courses"
            ) +
            "<div class='container'>"
            "<div class='error'>"
            "Could not connect to student server."
            "</div>"
            "</div>" +
            html_footer()
        );

        return;
    }

    if (
        !reply.success()
    ) {

        send_response(
            client_socket,
            html_header(
                "Courses"
            ) +
            "<div class='container'>"
            "<div class='error'>" +
            reply.message() +
            "</div>"
            "</div>" +
            html_footer()
        );

        return;
    }

    std::stringstream html;

    html
        << html_header(
            "Courses"
        )

        << "<div class='container'>"

        << "<h1>Available Courses</h1>"

        << "<div class='nav'>";

    if (
        session.role ==
        "admin"
    ) {

        html
            << "<a href='/admin'>"
            << "Admin"
            << "</a>";
    }

    if (
        session.role ==
        "student"
    ) {

        html
            << "<a href='/grades'>"
            << "My Grades"
            << "</a>";
    }

    if (
        session.role ==
        "faculty"
    ) {

        html
            << "<a href='/faculty'>"
            << "Faculty"
            << "</a>";
    }

    html
        << "<a href='/logout'>"
        << "Logout"
        << "</a>"

        << "</div>";


    for (
        const auto& course :
        reply.courses()
    ) {

        html
            << "<div class='course'>"

            << "<h2>"
            << course.course_code()
            << "</h2>"

            << "<p>"
            << course.course_name()
            << "</p>"

            << "<p>"
            << "<strong>Available slots:</strong> "
            << course.slots()
            << "</p>";

        if (
            session.role ==
            "student"
        ) {

            if (
                course.slots() > 0
            ) {

                html
                    << "<form method='POST' "
                       "action='/enroll'>"

                    << "<input "
                       "type='hidden' "
                       "name='course_code' "
                       "value='" +
                       course.course_code() +
                       "'>"

                    << "<button type='submit'>"
                    << "Enroll"
                    << "</button>"

                    << "</form>";
            }

            else {

                html
                    << "<p>"
                    << "<strong>No slots available.</strong>"
                    << "</p>";
            }
        }

        html
            << "</div>";
    }

    html
        << "</div>"

        << html_footer();

    send_response(
        client_socket,
        html.str()
    );
}


// ============================================================
// Enroll
// ============================================================

void handle_enroll(
    SOCKET client_socket,
    const WebSession& session,
    const std::string& body)
{
    if (
        session.role !=
        "student"
    ) {

        send_redirect(
            client_socket,
            "/courses"
        );

        return;
    }

    auto form =
        parse_form(body);

    auto stub =
        create_student_stub();

    enrollment::EnrollRequest request;

    request.set_session_token(
        session.grpc_token
    );

    request.set_course_code(
        form["course_code"]
    );

    enrollment::EnrollReply reply;

    ClientContext context;

    Status status =
        stub->Enroll(
            &context,
            request,
            &reply
        );

    if (
        !status.ok()
    ) {

        send_response(
            client_socket,
            html_header(
                "Enrollment"
            ) +
            "<div class='container'>"
            "<div class='error'>"
            "Could not connect to student server."
            "</div>"
            "<a href='/courses'>Back to Courses</a>"
            "</div>" +
            html_footer()
        );

        return;
    }

    std::stringstream html;

    html
        << html_header(
            "Enrollment"
        )

        << "<div class='container'>"

        << "<h1>Enrollment</h1>"

        << "<div class='"
        << (
            reply.success()
                ? "success"
                : "error"
        )
        << "'>"
        << reply.message()
        << "</div>"

        << "<a href='/courses'>"
        << "Back to Courses"
        << "</a>"

        << "</div>"

        << html_footer();

    send_response(
        client_socket,
        html.str()
    );
}


// ============================================================
// Faculty dashboard
// ============================================================

void handle_faculty(
    SOCKET client_socket,
    const WebSession& session,
    const std::string& message = "",
    bool error = false)
{
    if (
        session.role !=
        "faculty"
    ) {

        if (
            session.role ==
            "admin"
        ) {

            send_redirect(
                client_socket,
                "/admin"
            );
        }

        else {

            send_redirect(
                client_socket,
                "/courses"
            );
        }

        return;
    }

    std::stringstream html;

    html
        << html_header(
            "Faculty Dashboard"
        )

        << "<div class='container'>"

        << "<h1>Faculty Dashboard</h1>"

        << "<div class='nav'>"

        << "<a href='/faculty'>"
        << "Faculty"
        << "</a>"

        << "<a href='/courses'>"
        << "Courses"
        << "</a>"

        << "<a href='/logout'>"
        << "Logout"
        << "</a>"

        << "</div>";

    if (!message.empty()) {

        html
            << "<div class='"
            << (
                error
                    ? "error"
                    : "success"
            )
            << "'>"
            << message
            << "</div>";
    }


    // ========================================================
    // Upload grade
    // ========================================================

    html
        << "<h2>Upload Grade</h2>"

        << "<div class='info'>"
        << "Grades must be between 0 and 100. "
        << "If a grade already exists for the student and course, "
        << "the grade will be updated."
        << "</div>"

        << "<form method='POST' "
           "action='/faculty/upload-grade'>"

        << "<label>Student Username</label>"

        << "<input "
           "type='text' "
           "name='student_username' "
           "placeholder='Example: student1' "
           "required>"

        << "<label>Course Code</label>"

        << "<input "
           "type='text' "
           "name='course_code' "
           "placeholder='Example: CS101' "
           "required>"

        << "<label>Grade</label>"

        << "<input "
           "type='number' "
           "name='grade' "
           "min='0' "
           "max='100' "
           "step='0.01' "
           "placeholder='Example: 92' "
           "required>"

        << "<button type='submit'>"
        << "Upload Grade"
        << "</button>"

        << "</form>"

        << "</div>"

        << html_footer();

    send_response(
        client_socket,
        html.str()
    );
}


// ============================================================
// Upload grade
// ============================================================

void handle_upload_grade(
    SOCKET client_socket,
    const WebSession& session,
    const std::string& body)
{
    if (
        session.role !=
        "faculty"
    ) {

        if (
            session.role ==
            "admin"
        ) {

            send_redirect(
                client_socket,
                "/admin"
            );
        }

        else {

            send_redirect(
                client_socket,
                "/courses"
            );
        }

        return;
    }

    auto form =
        parse_form(body);

    double grade = 0.0;

    try {

        grade =
            std::stod(
                form["grade"]
            );
    }

    catch (...) {

        handle_faculty(
            client_socket,
            session,
            "Invalid grade.",
            true
        );

        return;
    }

    auto stub =
        create_faculty_stub();

    enrollment::UploadGradeRequest request;

    request.set_session_token(
        session.grpc_token
    );

    request.set_student_username(
        form["student_username"]
    );

    request.set_course_code(
        form["course_code"]
    );

    request.set_grade(
        grade
    );

    enrollment::UploadGradeReply reply;

    ClientContext context;

    Status status =
        stub->UploadGrade(
            &context,
            request,
            &reply
        );

    if (
        !status.ok()
    ) {

        handle_faculty(
            client_socket,
            session,
            "Could not connect to faculty server.",
            true
        );

        return;
    }

    handle_faculty(
        client_socket,
        session,
        reply.message(),
        !reply.success()
    );
}


// ============================================================
// Student grades page
// ============================================================

void handle_grades(
    SOCKET client_socket,
    const WebSession& session)
{
    if (
        session.role !=
        "student"
    ) {

        if (
            session.role ==
            "faculty"
        ) {

            send_redirect(
                client_socket,
                "/faculty"
            );
        }

        else if (
            session.role ==
            "admin"
        ) {

            send_redirect(
                client_socket,
                "/admin"
            );
        }

        else {

            send_redirect(
                client_socket,
                "/courses"
            );
        }

        return;
    }

    auto stub =
        create_student_stub();

    enrollment::GetGradesRequest request;

    request.set_session_token(
        session.grpc_token
    );

    enrollment::GetGradesReply reply;

    ClientContext context;

    Status status =
        stub->GetGrades(
            &context,
            request,
            &reply
        );

    if (
        !status.ok()
    ) {

        send_response(
            client_socket,
            html_header(
                "My Grades"
            ) +
            "<div class='container'>"
            "<div class='error'>"
            "Could not connect to student server."
            "</div>"
            "<a href='/courses'>"
            "Back to Courses"
            "</a>"
            "</div>" +
            html_footer()
        );

        return;
    }

    if (
        !reply.success()
    ) {

        send_response(
            client_socket,
            html_header(
                "My Grades"
            ) +
            "<div class='container'>"
            "<div class='error'>" +
            reply.message() +
            "</div>"
            "<a href='/courses'>"
            "Back to Courses"
            "</a>"
            "</div>" +
            html_footer()
        );

        return;
    }

    std::stringstream html;

    html
        << html_header(
            "My Grades"
        )

        << "<div class='container'>"

        << "<h1>My Grades</h1>"

        << "<div class='nav'>"

        << "<a href='/courses'>"
        << "Courses"
        << "</a>"

        << "<a href='/grades'>"
        << "My Grades"
        << "</a>"

        << "<a href='/logout'>"
        << "Logout"
        << "</a>"

        << "</div>";

    if (
        reply.grades_size() == 0
    ) {

        html
            << "<div class='info'>"
            << "No grades available yet."
            << "</div>";
    }

    else {

        html
            << "<table>"

            << "<tr>"
            << "<th>Course Code</th>"
            << "<th>Course Name</th>"
            << "<th>Grade</th>"
            << "</tr>";

        for (
            const auto& grade :
            reply.grades()
        ) {

            html
                << "<tr>"

                << "<td>"
                << grade.course_code()
                << "</td>"

                << "<td>"
                << grade.course_name()
                << "</td>"

                << "<td>"
                << std::fixed
                << std::setprecision(2)
                << grade.grade()
                << "</td>"

                << "</tr>";
        }

        html
            << "</table>";
    }

    html
        << "</div>"

        << html_footer();

    send_response(
        client_socket,
        html.str()
    );
}


// ============================================================
// Logout
// ============================================================

void handle_logout(
    SOCKET client_socket,
    const WebSession& session,
    const std::string& web_session_id)
{
    Status status;

    // ========================================================
    // Logout through the service that owns the user's role
    // ========================================================

    if (
        session.role ==
        "admin"
    ) {

        auto stub =
            create_admin_stub();

        enrollment::LogoutRequest request;

        request.set_session_token(
            session.grpc_token
        );

        enrollment::LogoutReply reply;

        ClientContext context;

        status =
            stub->Logout(
                &context,
                request,
                &reply
            );
    }

    else if (
        session.role ==
        "faculty"
    ) {

        auto stub =
            create_faculty_stub();

        enrollment::LogoutRequest request;

        request.set_session_token(
            session.grpc_token
        );

        enrollment::LogoutReply reply;

        ClientContext context;

        status =
            stub->Logout(
                &context,
                request,
                &reply
            );
    }

    else {

        auto stub =
            create_student_stub();

        enrollment::LogoutRequest request;

        request.set_session_token(
            session.grpc_token
        );

        enrollment::LogoutReply reply;

        ClientContext context;

        status =
            stub->Logout(
                &context,
                request,
                &reply
            );
    }


    remove_web_session(
        web_session_id
    );

    std::string headers =
        "Set-Cookie: session=; "
        "Expires=Thu, 01 Jan 1970 00:00:00 GMT; "
        "HttpOnly; SameSite=Lax\r\n";

    std::string response =
        "HTTP/1.1 302 Found\r\n"
        "Location: /\r\n" +
        headers +
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
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
// Handle client
// ============================================================

void handle_client(
    SOCKET client_socket)
{
    char buffer[65536];

    int received =
        recv(
            client_socket,
            buffer,
            sizeof(buffer) - 1,
            0
        );

    if (
        received <= 0
    ) {

        closesocket(
            client_socket
        );

        return;
    }

    buffer[received] =
        '\0';

    std::string request(
        buffer
    );


    // ========================================================
    // Request method
    // ========================================================

    bool is_post =
        request.rfind(
            "POST ",
            0
        ) == 0;

    bool is_get =
        request.rfind(
            "GET ",
            0
        ) == 0;


    // ========================================================
    // Request path
    // ========================================================

    size_t path_start =
        request.find(' ');

    if (
        path_start ==
        std::string::npos
    ) {

        closesocket(
            client_socket
        );

        return;
    }

    ++path_start;

    size_t path_end =
        request.find(
            ' ',
            path_start
        );

    if (
        path_end ==
        std::string::npos
    ) {

        closesocket(
            client_socket
        );

        return;
    }

    std::string path =
        request.substr(
            path_start,
            path_end - path_start
        );


    // ========================================================
    // Request body
    // ========================================================

    std::string body;

    size_t body_start =
        request.find(
            "\r\n\r\n"
        );

    if (
        body_start !=
        std::string::npos
    ) {

        body =
            request.substr(
                body_start + 4
            );
    }


    // ========================================================
    // Session
    // ========================================================

    std::string web_session_id =
        get_cookie(
            request,
            "session"
        );

    WebSession session;

    bool logged_in =
        get_web_session(
            web_session_id,
            session
        );


    // ========================================================
    // POST /login
    // ========================================================

    if (
        is_post &&
        path == "/login"
    ) {

        handle_login(
            client_socket,
            body
        );

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // GET /
    // ========================================================

    if (
        is_get &&
        path == "/"
    ) {

        if (!logged_in) {

            handle_login_page(
                client_socket
            );
        }

        else if (
            session.role ==
            "admin"
        ) {

            send_redirect(
                client_socket,
                "/admin"
            );
        }

        else if (
            session.role ==
            "faculty"
        ) {

            send_redirect(
                client_socket,
                "/faculty"
            );
        }

        else {

            send_redirect(
                client_socket,
                "/courses"
            );
        }

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // Everything below this point requires login
    // ========================================================

    if (!logged_in) {

        send_redirect(
            client_socket,
            "/"
        );

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // GET /admin
    // ========================================================

    if (
        is_get &&
        path == "/admin"
    ) {

        handle_admin(
            client_socket,
            session
        );

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // POST /admin/add-student
    // ========================================================

    if (
        is_post &&
        path == "/admin/add-student"
    ) {

        handle_add_student(
            client_socket,
            session,
            body
        );

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // POST /admin/add-course
    // ========================================================

    if (
        is_post &&
        path == "/admin/add-course"
    ) {

        handle_add_course(
            client_socket,
            session,
            body
        );

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // GET /faculty
    // ========================================================

    if (
        is_get &&
        path == "/faculty"
    ) {

        handle_faculty(
            client_socket,
            session
        );

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // POST /faculty/upload-grade
    // ========================================================

    if (
        is_post &&
        path == "/faculty/upload-grade"
    ) {

        handle_upload_grade(
            client_socket,
            session,
            body
        );

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // GET /courses
    // ========================================================

    if (
        is_get &&
        path == "/courses"
    ) {

        handle_courses(
            client_socket,
            session
        );

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // POST /enroll
    // ========================================================

    if (
        is_post &&
        path == "/enroll"
    ) {

        handle_enroll(
            client_socket,
            session,
            body
        );

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // GET /grades
    // ========================================================

    if (
        is_get &&
        path == "/grades"
    ) {

        handle_grades(
            client_socket,
            session
        );

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // GET /logout
    // ========================================================

    if (
        is_get &&
        path == "/logout"
    ) {

        handle_logout(
            client_socket,
            session,
            web_session_id
        );

        closesocket(
            client_socket
        );

        return;
    }


    // ========================================================
    // Unknown route
    // ========================================================

    send_response(
        client_socket,
        html_header(
            "404"
        ) +
        "<div class='container'>"
        "<h1>404</h1>"
        "<p>Page not found.</p>"
        "<a href='/'>"
        "Return to Login"
        "</a>"
        "</div>" +
        html_footer(),
        "404 Not Found"
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
    // Load configuration
    // ========================================================

    if (
        !load_config(
            "config.ini",
            server_config
        )
    ) {

        std::cerr
            << "ERROR: Could not load config.ini."
            << std::endl;

        return 1;
    }


    std::cout
        << "========================================"
        << std::endl;

    std::cout
        << "     DISTRIBUTED ENROLLMENT WEB SERVER"
        << std::endl;

    std::cout
        << "========================================"
        << std::endl;

    std::cout
        << "Web server: "
        << make_address(
            server_config.web_server_ip,
            server_config.web_server_port
        )
        << std::endl;

    std::cout
        << "Admin/Core server: "
        << make_address(
            server_config.admin_server_ip,
            server_config.admin_server_port
        )
        << std::endl;

    std::cout
        << "Student server: "
        << make_address(
            server_config.student_server_ip,
            server_config.student_server_port
        )
        << std::endl;

    std::cout
        << "Faculty server: "
        << make_address(
            server_config.faculty_server_ip,
            server_config.faculty_server_port
        )
        << std::endl;


    // ========================================================
    // Start Winsock
    // ========================================================

    WSADATA wsa_data;

    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsa_data
        ) != 0
    ) {

        std::cerr
            << "WSAStartup failed."
            << std::endl;

        return 1;
    }


    SOCKET server_socket =
        socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        );

    if (
        server_socket ==
        INVALID_SOCKET
    ) {

        std::cerr
            << "Failed to create socket."
            << std::endl;

        WSACleanup();

        return 1;
    }


    int reuse = 1;

    setsockopt(
        server_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(
            &reuse
        ),
        sizeof(reuse)
    );


    sockaddr_in server_address{};

    server_address.sin_family =
        AF_INET;

    server_address.sin_addr.s_addr =
        INADDR_ANY;

    server_address.sin_port =
        htons(
            server_config.web_server_port
        );


    if (
        bind(
            server_socket,
            reinterpret_cast<sockaddr*>(
                &server_address
            ),
            sizeof(server_address)
        ) == SOCKET_ERROR
    ) {

        std::cerr
            << "Failed to bind to port "
            << server_config.web_server_port
            << "."
            << std::endl;

        closesocket(
            server_socket
        );

        WSACleanup();

        return 1;
    }


    if (
        listen(
            server_socket,
            SOMAXCONN
        ) == SOCKET_ERROR
    ) {

        std::cerr
            << "Failed to listen."
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
        << "[WEB] Server started successfully."
        << std::endl;

    std::cout
        << "[WEB] Address: "
        << make_address(
            server_config.web_server_ip,
            server_config.web_server_port
        )
        << std::endl;

    std::cout
        << "[WEB] Admin/Core: "
        << make_address(
            server_config.admin_server_ip,
            server_config.admin_server_port
        )
        << std::endl;

    std::cout
        << "[WEB] Student: "
        << make_address(
            server_config.student_server_ip,
            server_config.student_server_port
        )
        << std::endl;

    std::cout
        << "[WEB] Faculty: "
        << make_address(
            server_config.faculty_server_ip,
            server_config.faculty_server_port
        )
        << std::endl;


    // ========================================================
    // Accept connections
    // ========================================================

    while (true) {

        sockaddr_in client_address{};

        int client_address_size =
            sizeof(client_address);

        SOCKET client_socket =
            accept(
                server_socket,
                reinterpret_cast<sockaddr*>(
                    &client_address
                ),
                &client_address_size
            );

        if (
            client_socket ==
            INVALID_SOCKET
        ) {

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