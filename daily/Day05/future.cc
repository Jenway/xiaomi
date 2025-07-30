#include <future>
#include <iostream>
#include <thread>
template <typename T>
void printFuture(std::future<T>& fut)
{
    std::cout << "Waiting for future...\n";
    T result = fut.get();
    std::cout << "Future completed with result: " << result << "\n";
}

int main()
{
    auto fut = std::async(std::launch::async, []() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 42;
    });

    printFuture(fut);

    std::promise<int> prom;
    auto fut2 = prom.get_future();
    std::thread t([&prom]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        prom.set_value(84);
    });

    printFuture(fut2);
    t.join();
    return 0;
}