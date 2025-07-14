#pragma once
#include "ConcurrentQueue.hpp"
#include <chrono>
#include <future>
#include <ostream>
#include <string>

namespace ProducerDemo {
struct Product {
    std::string name;
    int32_t id;
    double price;
    friend std::ostream& operator<<(std::ostream& os, const Product& product)
    {
        return os << "Product{id: " << product.id << ", name: '" << product.name << "', price: " << product.price << "}";
    }
};

struct ProducerConfig {
    std::chrono::milliseconds delay;
    int32_t max_produce;
};

struct ConsumerConfig {
    std::chrono::milliseconds delay;
    int32_t max_consume;
};

void producer(int32_t id, ConcurrentQueue<std::future<Product>>& queue, const ProducerConfig& config);
void consumer(int32_t id, ConcurrentQueue<std::future<Product>>& queue, const ConsumerConfig& config);

} // namespace ProducerDemo
