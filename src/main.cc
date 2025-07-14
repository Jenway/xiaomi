#include "ConcurrentQueue.hpp"
#include "Product.hpp"
#include "ScopeThread.hpp"
#include <future>
#include <vector>

using ProducerDemo::consumer;
using ProducerDemo::producer;
using ProducerDemo::Product;

constexpr int32_t BUFFER_SIZE = 5;
constexpr int32_t num_producers = 2;
constexpr int32_t num_consumers = 3;

constexpr ProducerDemo::ProducerConfig producer_config {
    .delay = std::chrono::milliseconds(1000),
    .max_produce = 9
};
constexpr ProducerDemo::ConsumerConfig consumer_config {
    .delay = std::chrono::milliseconds(1500),
    .max_consume = 6
};

int main()
{

    ConcurrentQueue<std::future<Product>> product_queue(BUFFER_SIZE);

    // ScopedThread is a custom thread RAII Wrapper
    std::vector<ScopedThread> producers;
    std::vector<ScopedThread> consumers;

    producers.reserve(num_producers);
    consumers.reserve(num_consumers);

    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back(
            std::thread([i, &product_queue]() {
                producer(i + 1, product_queue, producer_config);
            }));
    }

    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back(
            std::thread([i, &product_queue]() {
                consumer(i + 1, product_queue, consumer_config);
            }));
    }

    return 0;
}