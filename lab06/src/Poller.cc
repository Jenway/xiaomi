#include "Poller.hpp"

void Poller::add_fd(int fd, uint32_t events) const
{
    epoll_event ev {};
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        throw std::runtime_error("epoll_ctl ADD failed");
    }
}

void Poller::remove_fd(int fd) const
{
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
}

Poller::EventRange Poller::wait(int timeout_ms)
{
    int n = epoll_wait(epfd_, events_.data(), max_events_, timeout_ms);
    if (n < 0) {
        throw std::runtime_error("epoll_wait failed");
    }
    return { events_, n };
}
