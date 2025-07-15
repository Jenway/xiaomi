
#pragma once
#include "Poller.hpp"
#include <cerrno>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/sendfile.h>
#include <unistd.h>

namespace tcp {

static constexpr int BUF_SIZE = 4096;

class Connection {
public:
    explicit Connection(int fd)
        : fd_(fd)
    {
    }

    [[nodiscard]] int fd() const { return fd_; }

    void handle(Poller& poller);

private:
    int fd_;
    std::string recv_buf_;

    void send_file_response(const std::string& filename);

    void send_error(const std::string& msg);

    void close_self(Poller& poller) const;
};

}