#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

constexpr std::chrono::milliseconds PRODUCER_SLEEP_TIME(2000);

class semp {

private:
    int cnt = 0;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void accquire()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return cnt > 0; });
        --cnt;
        // std::cout << "Semaphore acquired, remaining count: " << cnt << "\n";
    }
    void release()
    {
        std::lock_guard<std::mutex> lock(mtx);
        ++cnt;
        // std::cout << "Semaphore released, new count: " << cnt << "\n";
        cv.notify_one();
    }
};

int main()
{
    /*
    生产者与消费者模型
    线程A是消费者，线程B是消费者，线程C是生产者
    当线程C生产成功后，通知线程A和线程B消费
    思考一下：除了打印通知外，是不是可以打印出模拟生产和消费的个数
    */
    auto semp_instance = semp();

    auto producer = [&semp_instance](int id) {
        int produced = 0;
        while (true) {
            semp_instance.release();
            ++produced;
            std::stringstream ss;
            ss << "[Producer " << id << "] Produced item #" << produced << "\n";
            std::cout << ss.str();
            std::this_thread::sleep_for(PRODUCER_SLEEP_TIME);
        }
    };

    auto consumer = [&semp_instance](int id) {
        int consumed = 0;
        while (true) {
            semp_instance.accquire();
            ++consumed;
            std::stringstream ss;
            ss << "[Consumer " << id << "] Consumed item #" << consumed << "\n";
            std::cout << ss.str();
        }
    };

    auto consumers = std::vector<std::thread>();
    auto producers = std::vector<std::thread>();
    for (int i = 0; i < 10; ++i) {
        consumers.emplace_back(consumer, i + 1);
    }
    for (int i = 0; i < 3; ++i) {
        producers.emplace_back(producer, i + 1);
    }
    for (auto& t : consumers) {
        t.join();
    }
    for (auto& t : producers) {
        t.join();
    }
    return 0;
}