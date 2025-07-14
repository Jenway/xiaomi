#include "Product.hpp"
#include <future>
#include <iostream>
#include <sstream>
#include <thread>

namespace ProducerDemo {

void consumer(int32_t id, ConcurrentQueue<std::future<Product>>& queue, const ConsumerConfig& config)
{
    std::stringstream ss;

    for (int32_t consumed = 0; consumed < config.max_consume; ++consumed) {
        auto future = queue.pop();

        ss.str("");
        ss << "Consumer " << id << ": Received a future. Queue size approx: " << queue.size() << '\n';
        std::cout << ss.str();

        std::this_thread::sleep_for(config.delay);

        Product p = future.get();

        ss.str("");
        ss << "Consumer " << id << ": Consumed product " << p << '\n';
        std::cout << ss.str();
    }

    ss.str("");
    ss << "Consumer " << id << ": Finished consuming " << config.max_consume << " items.\n";
    std::cout << ss.str();
}

} // namespace ProducerDemo
