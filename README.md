## Day 04

实现一个生产者和消费者模型，通过 `promise` 和 `future` 去传递生产商品的信息，至少包括商品名称，编号和价格

run `bash run.sh`

### 实现

我们通过一个线程安全的队列包装类来作为生产者和消费者共同的中介，通过保证并发队列类 `push`、`pop`的线程安全性，保证生产者和消费者模型的稳定。

`ConcurrentQueue` 的存储类型参数为 `std::future<Product>`,生产者和消费者装箱开箱即可。



我们通过一个线程安全的并发队列 `ConcurrentQueue` 作为生产者与消费者之间的通信中介，确保在多线程环境下的稳定性和正确性。

队列的存储类型为 `std::future<Product>`，生产者通过 `promise.set_value` 装箱，消费者则通过 `future.get` 开箱得到结果。

### 并发队列实现

- **互斥访问：** 使用 `std::mutex` 保证多个线程不会同时修改队列；
- **容量控制：** 使用两个自定义信号量：
  - `empty_slots` 表示还有多少空间可供入队；
  - `filled_slots` 表示当前可出队的元素数量；
  
这使得并发队列的 `push` 和 `pop` 是线程安全的（大概），有效避免忙等或竞争条件。

自定义信号量只需要一个 `std::mutex` 一个 `std::condition_variable`和一个普通整型变量即可实现。

### 运行结果

我们这里设置：

- 队列长度有限，为 5
- 生产者数量为 2，每个生产者最多生产 9 件物品，每隔 1 s 生成
- 消费者数量为 3，每个生产者最多生产 6 件物品，每隔 1.5 s 消费

结果如下：

![alt text](static/output_00.png)

log 太长就不贴完整了，总之：

- 这个模型的行为是比较合理的，确保了生产者的生产和消费者消费吻合；

![alt text](static/output_01.png)

### 文件结构

```bash
❯ exa -T
.
├── CMakeLists.txt
├── include
│  ├── ConcurrentQueue.hpp
│  ├── Product.hpp
│  ├── ScopeThread.hpp
│  └── Semp.hpp
├── README.md
├── run.sh
├── src
│  ├── Consumer.cc
│  ├── main.cc
│  └── Producer.cc
└── static
   ├── output_00.png
   └── output_01.png
```