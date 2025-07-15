#pragma once
#include <arpa/inet.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <unistd.h>

namespace tcp {
class Socket {
private:
    int32_t _fd;
    int32_t _port;
    std::atomic<bool> _running { true };

    void bind() const;
    void listen() const;

public:
    explicit Socket(int32_t port);
    ~Socket();

    [[nodiscard]] int32_t fd() const { return _fd; }
    [[nodiscard]] int32_t accept() const;
};

} // namespace tcp
