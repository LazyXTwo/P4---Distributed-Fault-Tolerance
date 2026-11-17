#include <iostream>
#include <string>
#include <thread>
#include <sstream>
#include <iomanip>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <grpcpp/grpcpp.h>

#include "enrollment.grpc.pb.h"


#pragma comment(lib, "ws2_32.lib")


using enrollment::EnrollmentService;
using enrollment::LoginRequest;
using enrollment::LoginReply;


// ============================================================
// gRPC backend
// ============================================================

std::shared_ptr<grpc::Channel> backend_channel;

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

    for (size_t i = 0; i < value.length(); i++)
    {
        if (value[i] == '%'
            && i + 2 < value.length())
        {
            int number;

            std::stringstream stream;

            stream
                << std::hex
                << value.substr(i + 1, 2);

            stream >> number;

            result += static_cast<char>(number);

            i += 2;
        }
        else if (value[i] == '+')
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
        body.find(search);

    if (position == std::string::npos)
    {
        return "";
    }

    position += search.length();

    size_t end =
        body.find(
            '&',
            position
        );

    std::string value;

    if (end == std::string::npos)
    {
        value =
            body.substr(position);
    }
    else
    {
        value =
            body.substr(
                position,
                end - position
            );
    }

    return url_decode(value);
}


// ============================================================
// Send HTTP response
// ============================================================

void send_response(
    SOCKET client_socket,
    const std::string& body
)
{
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " +
        std::to_string(body.size()) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n"
        +
        body;

    send(
        client_socket,
        response.c_str(),
        static_cast<int>(response.size()),
        0
    );
}


// ============================================================
// Login through gRPC
// ============================================================

std::string login_user(
    const std::string& username,
    const std::string& password
)
{
    auto stub =
        create_stub();


    LoginRequest request;

    request.set_username(
        username
    );

    request.set_password(
        password
    );


    LoginReply reply;

    grpc::ClientContext context;


    grpc::Status status =
        stub->Login(
            &context,
            request,
            &reply
        );


    // --------------------------------------------------------
    // gRPC connection failed
    // --------------------------------------------------------

    if (!status.ok())
    {
        std::cout
            << "gRPC Login failed."
            << std::endl;

        std::cout
            << "Error: "
            << status.error_message()
            << std::endl;


        return
            "<h2>Backend unavailable</h2>"
            "<p>The enrollment server could not be reached.</p>"
            "<a href=\"/\">Back to login</a>";
    }


    // --------------------------------------------------------
    // Login successful
    // --------------------------------------------------------

    if (reply.success())
    {
        std::cout
            << "Login successful: "
            << username
            << std::endl;


        return
            "<h2>Login successful</h2>"
            "<p>Welcome, " +
            username +
            "!</p>"
            "<p>Session token received.</p>"
            "<a href=\"/\">Back to login</a>";
    }


    // --------------------------------------------------------
    // Login rejected
    // --------------------------------------------------------

    std::cout
        << "Login rejected: "
        << username
        << std::endl;


    return
        "<h2>Login failed</h2>"
        "<p>" +
        reply.message() +
        "</p>"
        "<a href=\"/\">Try again</a>";
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


    if (bytes_received <= 0)
    {
        closesocket(
            client_socket
        );

        return;
    }


    buffer[bytes_received] =
        '\0';


    std::string request(
        buffer
    );


    std::cout
        << "HTTP request received."
        << std::endl;


    // ========================================================
    // POST /login
    // ========================================================

    if (
        request.rfind(
            "POST /login",
            0
        ) == 0
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


        std::string result =
            login_user(
                username,
                password
            );


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

)"
            +
            result
            +
            R"(

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


    // ========================================================
    // GET /
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
    // Connect to gRPC backend
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


    // ========================================================
    // Server information
    // ========================================================

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


        // ----------------------------------------------------
        // Handle each browser connection separately
        // ----------------------------------------------------

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