#include "ClientConnection.hpp"
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <stdexcept>

using json = nlohmann::json;

namespace tcp {

ClientConnection::ClientConnection(const std::string& host, int port)
{
    addrinfo hints {};
    addrinfo* res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);
    if (err != 0) {
        throw std::runtime_error("getaddrinfo failed");
    }

    fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    if (connect(fd_, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd_);
        throw std::runtime_error("connect failed");
    }

    freeaddrinfo(res);
}

bool ClientConnection::readLine(std::string& line)
{
    char ch;
    line.clear();
    while (true) {
        ssize_t n = read(fd_, &ch, 1);
        if (n <= 0)
            return false;
        line.push_back(ch);
        if (ch == '\n')
            break;
    }
    return true;
}

bool ClientConnection::readExact(std::vector<char>& buffer, size_t size)
{
    buffer.resize(size);
    size_t received = 0;
    while (received < size) {
        ssize_t n = read(fd_, &buffer[received], size - received);
        if (n <= 0)
            return false;
        received += n;
    }
    return true;
}

bool ClientConnection::requestFile(const std::string& filename, const std::string& output_path)
{
    json req = { { "filename", filename } };
    std::string req_str = req.dump() + "\n";
    write(fd_, req_str.data(), req_str.size());

    std::string header;
    if (!readLine(header))
        return false;

    try {
        json resp = json::parse(header);
        if (resp.contains("error")) {
            std::cerr << "Server error: " << resp["error"] << "\n";
            return false;
        }

        size_t fsize = resp["filesize"];
        std::vector<char> file_data;
        if (!readExact(file_data, fsize)) {
            std::cerr << "Failed to read file data\n";
            return false;
        }

        std::ofstream ofs(output_path, std::ios::binary);
        ofs.write(file_data.data(), file_data.size());
        std::cout << "File saved to " << output_path << "\n";
        return true;

    } catch (...) {
        std::cerr << "Invalid response from server\n";
        return false;
    }
}

} // namespace tcp
