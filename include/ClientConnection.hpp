#pragma once
#include <string>
#include <unistd.h>
#include <vector>

namespace tcp {

class ClientConnection {
public:
    ClientConnection(const std::string& host, int port);
    bool requestFile(const std::string& filename, const std::string& output_path);

private:
    int fd_;

    bool readLine(std::string& line);
    bool readExact(std::vector<char>& buffer, size_t size);
};

} // namespace tcp
