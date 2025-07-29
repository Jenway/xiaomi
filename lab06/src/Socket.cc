#include "Socket.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace tcp {
namespace {
    int set_nonblocking(int fd)
    {
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
            return -1;
        }
        return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

} // namespace

void Socket::bind() const
{
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(_fd);
        throw std::runtime_error("Failed to set socket options");
    }

    if (::bind(_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(_fd);
        throw std::runtime_error("Failed to bind socket");
    }
}

void Socket::listen() const
{
    if (::listen(_fd, SOMAXCONN) < 0) {
        close(_fd);
        throw std::runtime_error("Failed to listen on socket");
    }
}

Socket::Socket(int32_t port)
    : _fd(socket(AF_INET, SOCK_STREAM, 0))
    , _port(port)
{

    if (_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    set_nonblocking(_fd);
    bind();
    listen();
}

Socket::~Socket()
{
    if (_fd >= 0) {
        close(_fd);
    }
}

int32_t Socket::accept() const
{
    sockaddr_in client_addr {};
    socklen_t client_len = sizeof(client_addr);

    int32_t client_fd = ::accept(_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 非阻塞下，当前没有新连接
            return -1;
        }
        throw std::system_error(errno, std::generic_category(), "Socket accept failed");
    }

    set_nonblocking(client_fd);

    // BEGIN LOG New Client
    char ip_buf[INET_ADDRSTRLEN];
    const char* ip = inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf));
    uint16_t port = ntohs(client_addr.sin_port);

    std::cout << "New Client: " << ((ip != nullptr) ? ip : "Unknown") << ":" << port << "\n";
    // END LOG New Client

    return client_fd;
}

} // namespace tcp
