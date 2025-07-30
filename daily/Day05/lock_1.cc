
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
std::mutex mtx;

void print_even(int x)
{
    if (x % 2 == 0) {
        std::stringstream ss;
        ss << x << " is even.\n";
        std::cout << ss.str();
    } else {
        throw(std::logic_error("Not an even number: " + std::to_string(x)));
    }
}

void print_thread_id(int id)
{
    try {
        std::lock_guard<std::mutex> lock(mtx);
        print_even(id);

    } catch (const std::logic_error& e) {
        std::stringstream ss;
        ss << "Exception in thread " << id << ": " << e.what() << "\n";
        std::cerr << ss.str();
    }
}

int main()
{
    std::thread threads[10];
    for (int i = 0; i < 10; ++i) {
        threads[i] = std::thread(print_thread_id, i + 1);
    }
    for (auto& th : threads) {
        th.join();
    }
    return 0;
}