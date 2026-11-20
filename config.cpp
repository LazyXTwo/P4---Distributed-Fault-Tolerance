#include "config.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>


// ============================================================
// Remove spaces from the beginning and end of a string
// ============================================================

static std::string trim(const std::string& value)
{
    size_t start = 0;

    while (
        start < value.length() &&
        std::isspace(
            static_cast<unsigned char>(value[start])
        )
    )
    {
        start++;
    }

    size_t end = value.length();

    while (
        end > start &&
        std::isspace(
            static_cast<unsigned char>(value[end - 1])
        )
    )
    {
        end--;
    }

    return value.substr(start, end - start);
}


// ============================================================
// Load configuration from config.ini
// ============================================================

bool load_config(
    const std::string& filename,
    ServerConfig& config
)
{
    std::ifstream file(filename);

    if (!file.is_open())
    {
        std::cerr
            << "ERROR: Could not open configuration file: "
            << filename
            << std::endl;

        return false;
    }

    std::string line;

    while (std::getline(file, line))
    {
        line = trim(line);

        // Ignore empty lines
        if (line.empty())
        {
            continue;
        }

        // Ignore comments
        if (line[0] == '#' || line[0] == ';')
        {
            continue;
        }

        // Ignore section headers
        if (line.front() == '[' && line.back() == ']')
        {
            continue;
        }

        size_t equals_position = line.find('=');

        if (equals_position == std::string::npos)
        {
            continue;
        }

        std::string key =
            trim(
                line.substr(
                    0,
                    equals_position
                )
            );

        std::string value =
            trim(
                line.substr(
                    equals_position + 1
                )
            );


        // ========================================================
        // Web server
        // ========================================================

        if (key == "WEB_SERVER_IP")
        {
            config.web_server_ip = value;
        }
        else if (key == "WEB_SERVER_PORT")
        {
            config.web_server_port = std::stoi(value);
        }


        // ========================================================
        // Admin server
        // ========================================================

        else if (key == "ADMIN_SERVER_IP")
        {
            config.admin_server_ip = value;
        }
        else if (key == "ADMIN_SERVER_PORT")
        {
            config.admin_server_port = std::stoi(value);
        }


        // ========================================================
        // Student server
        // ========================================================

        else if (key == "STUDENT_SERVER_IP")
        {
            config.student_server_ip = value;
        }
        else if (key == "STUDENT_SERVER_PORT")
        {
            config.student_server_port = std::stoi(value);
        }


        // ========================================================
        // Faculty server
        // ========================================================

        else if (key == "FACULTY_SERVER_IP")
        {
            config.faculty_server_ip = value;
        }
        else if (key == "FACULTY_SERVER_PORT")
        {
            config.faculty_server_port = std::stoi(value);
        }
    }

    file.close();

    return true;
}


// ============================================================
// Create IP:PORT address
// ============================================================

std::string make_address(
    const std::string& ip,
    int port
)
{
    return ip + ":" + std::to_string(port);
}