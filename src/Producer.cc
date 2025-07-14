#include "Product.hpp"
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

namespace ProducerDemo {

void producer(int id, ConcurrentQueue<std::future<Product>>& queue, const ProducerConfig& config)
{
    std::random_device rd;
    std::mt19937 gen(rd() + (id * 997)); // 避免每个线程种子一致
    std::uniform_int_distribution<int> id_dist(10000, 99999);
    std::uniform_real_distribution<double> price_dist(10.0, 99.99);
    std::uniform_int_distribution<int> suffix_dist(100, 999);
    std::stringstream ss;

    for (int produced = 0; produced < config.max_produce; ++produced) {
        std::promise<Product> product_promise;
        std::future<Product> product_future = product_promise.get_future();

        queue.push(std::move(product_future));
        ss.str("");
        ss << "Producer " << id << ": Pushed a future. Queue size approx: " << queue.size() << '\n';
        std::cout << ss.str();

        std::this_thread::sleep_for(config.delay);

        int rand_id = id_dist(gen);
        double rand_price = price_dist(gen);
        int suffix = suffix_dist(gen);

        Product p = {
            .name = "Item-" + std::to_string(id) + "-" + std::to_string(suffix),
            .id = rand_id,
            .price = rand_price
        };
        ss.str("");
        ss << "Producer " << id << ": Produced product " << p << '\n';
        std::cout << ss.str();

        product_promise.set_value(p);
    }

    ss.str("");
    ss << "Producer " << id << ": Finished producing " << config.max_produce << " items.\n";
    std::cout << ss.str();
}

} // namespace ProducerDemo
