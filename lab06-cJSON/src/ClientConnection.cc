#include "ClientConnection.hpp"
#include "cJSON/cJSON.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <stdexcept>

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
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "filename", filename.c_str());
    char* req_str = cJSON_PrintUnformatted(req);
    std::string message = std::string(req_str) + "\n"; // 添加换行
    cJSON_free(req_str);
    cJSON_Delete(req);

    write(fd_, message.data(), message.size());

    std::string header;
    if (!readLine(header)) {
        return false;
    }

    try {
        cJSON* resp = cJSON_Parse(header.c_str());
        if (resp == nullptr) {
            std::cerr << "Invalid JSON response\n";
            return false;
        }
        cJSON* err = cJSON_GetObjectItemCaseSensitive(resp, "error");
        if (cJSON_IsString(err) != 0) {
            std::cerr << "Server error: " << err->valuestring << "\n";
            cJSON_Delete(resp);
            return false;
        }

        cJSON* fsize_json = cJSON_GetObjectItemCaseSensitive(resp, "filesize");
        if (cJSON_IsNumber(fsize_json) == 0) {
            std::cerr << "Missing or invalid 'filesize'\n";
            cJSON_Delete(resp);
            return false;
        }

        size_t fsize = static_cast<size_t>(fsize_json->valuedouble);
        cJSON_Delete(resp);

        // --- 接收文件内容 ---
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
