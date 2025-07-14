#pragma once
#include <thread>

class ScopedThread {
public:
    explicit ScopedThread(std::thread t)
        : m_thread(std::move(t))
    {
    }

    ~ScopedThread()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    ScopedThread(const ScopedThread&) = delete;
    ScopedThread& operator=(const ScopedThread&) = delete;

    ScopedThread(ScopedThread&&) = default;
    ScopedThread& operator=(ScopedThread&&) = default;

private:
    std::thread m_thread;
};
