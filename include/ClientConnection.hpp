#pragma once
#include "Socket.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <unistd.h>
#include <vector>

namespace tcp {

class ClientConnection {
public:
    ClientConnection(const std::string& host, int port);
    bool request_file(const std::string& filename, const std::string& output_path);

private:
    int fd_;

    bool read_line(std::string& line);
    bool read_exact(std::vector<char>& buffer, size_t size);
};

} // namespace tcp
