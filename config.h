#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct ServerConfig
{
    std::string web_server_ip;
    int web_server_port;

    std::string admin_server_ip;
    int admin_server_port;

    std::string student_server_ip;
    int student_server_port;

    std::string faculty_server_ip;
    int faculty_server_port;
};

// Loads configuration from config.ini
// Returns true if successful.
bool load_config(
    const std::string& filename,
    ServerConfig& config
);

// Converts IP + port into:
// IP:PORT
std::string make_address(
    const std::string& ip,
    int port
);

#endif