#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>

constexpr int32_t SPEED = 50;
constexpr int32_t SPEED_UI = 50;
constexpr int32_t MAX_STATUS = 100;

int main()
{
    std::mutex mtx;
    std::condition_variable cv;
    int32_t status = 0;

    std::jthread worker([&] {
        for (int i = 0; i <= MAX_STATUS; ++i) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                status = i;
            }
            cv.notify_one();
            std::this_thread::sleep_for(std::chrono::milliseconds(SPEED));
        }
    });

    int32_t last_shown = -1;
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::milliseconds(SPEED_UI), [&] {
            return status != last_shown;
        });

        if (status != last_shown) {
            std::cout << "\rStatus: " << std::setw(3) << status << "%" << std::flush;
            last_shown = status;
        }

        if (status >= MAX_STATUS) {
            break;
        }
    }

    std::cout << "\nDownload complete!\n";
    return 0;
}
