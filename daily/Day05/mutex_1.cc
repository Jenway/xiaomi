#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
std::timed_mutex time_mtx;

void firework()
{
    while (!time_mtx.try_lock_for(std::chrono::milliseconds(200))) {
        std::cout << "-";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "*" << "\n";
    time_mtx.unlock();
}

int main()
{
    int32_t process_rate = 0;
    std::thread threads[10];
    for (int i = 0; i < 10; ++i) {
        threads[i] = std::thread(firework);
    }
    for (auto& t : threads) {
        t.join();
    }
    return 0;
}