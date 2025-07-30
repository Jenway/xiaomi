#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

int main()
{
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
    auto threads = std::vector<std::thread>();
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&mtx, &cv, &ready, i]() {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&ready]() { return ready; });
            std::cout << "Thread " << i << " is running after condition is met.\n";
        });
    }
    {
        std::lock_guard<std::mutex> lock(mtx);
        std::cout << "Main thread is setting the condition.\n";
        ready = true;
    }
    cv.notify_all();
    for (auto& th : threads) {
        th.join();
    }
}
