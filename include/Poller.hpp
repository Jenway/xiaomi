#pragma once
#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

class Poller {
public:
    class EventRange {
    public:
        using iterator = std::vector<epoll_event>::const_iterator;

        EventRange(const std::vector<epoll_event>& events, int count)
            : events_(events)
            , count_(count)
        {
        }

        [[nodiscard]] iterator begin() const { return events_.begin(); }
        [[nodiscard]] iterator end() const { return events_.begin() + count_; }

    private:
        const std::vector<epoll_event>& events_;
        int count_;
    };

    explicit Poller(int max_events = 1024)
        : epfd_(epoll_create1(0))
        , max_events_(max_events)
        , events_(max_events)
    {
        if (epfd_ < 0) {
            throw std::runtime_error("Failed to create epoll instance");
        }
    }

    ~Poller()
    {
        if (epfd_ >= 0) {
            close(epfd_);
        }
    }

    void add_fd(int fd, uint32_t events) const;
    void remove_fd(int fd) const;

    EventRange wait(int timeout_ms = -1);

private:
    int epfd_;
    int max_events_;
    std::vector<epoll_event> events_;
};
