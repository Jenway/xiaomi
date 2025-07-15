
#include "Connection.hpp"

namespace tcp {
using json = nlohmann::json;

void Connection::handle(Poller& poller)
{
    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = read(fd_, buf, sizeof(buf))) > 0) {
        recv_buf_.append(buf, n);
        if (recv_buf_.find('\n') != std::string::npos)
            break;
    }

    if (n == 0 || (n < 0 && errno != EAGAIN)) {
        close_self(poller);
        return;
    }

    auto pos = recv_buf_.find('\n');
    if (pos != std::string::npos) {
        std::string line = recv_buf_.substr(0, pos);
        try {
            json j = json::parse(line);
            std::string filename = j.at("filename");
            send_file_response(filename);
        } catch (...) {
            send_error("invalid request");
        }

        close_self(poller);
    }
}

void Connection::send_file_response(const std::string& filename)
{
    int file_fd = ::open(filename.c_str(), O_RDONLY);
    if (file_fd < 0) {
        send_error("not file");
        return;
    }

    off_t offset = 0;
    off_t fsize = ::lseek(file_fd, 0, SEEK_END);
    ::lseek(file_fd, 0, SEEK_SET);

    json resp = { { "filesize", static_cast<size_t>(fsize) } };
    std::string hdr = resp.dump() + "\n";
    write(fd_, hdr.data(), hdr.size());

    while (offset < fsize) {
        ssize_t sent = ::sendfile(fd_, file_fd, &offset, fsize - offset);
        if (sent <= 0)
            break; // error or done
    }

    ::close(file_fd);
}

void Connection::send_error(const std::string& msg)
{
    json err = { { "error", msg } };
    std::string s = err.dump() + "\n";
    write(fd_, s.data(), s.size());
}

void Connection::close_self(Poller& poller) const
{
    poller.remove_fd(fd_);
    close(fd_);
}

} // namespace tcp
