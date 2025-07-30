#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>

constexpr int32_t SPEED = 50; // milliseconds per percent
constexpr int32_t SPEED_UI = 50; // milliseconds for UI update
constexpr int32_t MAX_STATUS = 100;

int main()
{
    std::atomic<int32_t> status = 0;

    std::jthread t2([&status]() {
        while (status < MAX_STATUS) {
            status++;
            std::this_thread::sleep_for(std::chrono::milliseconds(SPEED));
        }
        std::cout << "\nDownload complete!\n";
    });

    while (status < MAX_STATUS) {
        std::cout << "\rStatus: " << std::setw(3) << status << "%" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(SPEED_UI));
    }

    return 0;
}
