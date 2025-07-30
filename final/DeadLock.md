**这是一个经典的死锁（Deadlock）问题**。您的 FSM 线程（`NativePlayerFSM`）和 Mp4Parser 的内部线程陷入了互相等待的僵局。

### 死锁分析

让我们一步步分解死锁是如何发生的：

1.  **解码器线程阻塞：**
    *   在正常播放时，`Mp4Parser` 的解码器线程（`Decoder` 内部的 `std::thread`）从 `video_packet_queue_` 取出数据包，解码成 `AVFrame`。
    *   然后，它通过回调函数 `callbacks.on_video_frame_decoded` 将解码后的帧**推送**到 `NativePlayer` 的 `video_frame_queue_` 中。
    *   如果此时 `NativePlayer` 的 `video_frame_queue_` **满了**（因为 FSM 线程可能暂时繁忙或等待同步），解码器线程在调用 `video_frame_queue_->push(...)` 时就会**被阻塞**，等待队列空出位置。

2.  **用户发起 Seek 操作：**
    *   您的 FSM 线程 (`TID: ...576`) 接收到 `SEEK` 命令，并调用 `mp4parser->seek(...)`。
    *   这个调用会向 `Mp4Parser` 的 `command_queue_` 发送一个 `SEEK` 命令。
    *   紧接着，您的 FSM 线程调用 `future.wait()`，开始**阻塞**，等待 `Mp4Parser` 完成 Seek 操作并满足 `promise`。

3.  **Mp4Parser 控制线程开始 Seek：**
    *   `Mp4Parser` 的控制线程 (`TID: ...722`) 从队列中取出 `SEEK` 命令，开始执行 `handle_seek` 函数。

4.  **死锁形成：**
    *   在 `handle_seek` 函数内部，代码执行到：
      ```cpp
      LOGI("Seek: Stopping decoders...");
      if (video_decoder_)
          video_decoder_->Stop(); // <-- 致命点 1
      ```
    *   `video_decoder_->Stop()` 内部几乎可以肯定会调用 `decoder_thread.join()`，它会**阻塞**，等待解码器线程执行完毕。
    *   但是，正如第 1 步所述，解码器线程**已经被阻塞**了，因为它在等待 `NativePlayer` 的 `video_frame_queue_` 空出位置。
    *   而唯一能让 `video_frame_queue_` 空出位置的，是 `NativePlayer` 的 FSM 线程。但 FSM 线程也**已经被阻塞**了，因为它在等待 `Mp4Parser` 的 `handle_seek` 函数执行完毕。

**依赖闭环形成：**

*   **FSM 线程** 正在等待 **Mp4Parser 控制线程**。
*   **Mp4Parser 控制线程** 正在等待 **解码器线程** (`join`)。
*   **解码器线程** 正在等待 **FSM 线程** (消费队列)。

所有线程都在等待对方，没有人能继续前进，程序就卡死了。

### 解决方案：打破依赖循环

解决死锁的关键是打破这个等待环。核心思想是：**必须让解码器线程在被要求停止时，能够无条件地、立即地从阻塞状态中唤醒并退出**。

这需要在你的 `SemQueue` 和 `handle_seek` 逻辑中进行修改。

#### 步骤 1: 升级你的 `SemQueue` (最关键的一步)

你的 `SemQueue` 必须支持一个“关闭”或“唤醒所有”的机制。当队列被关闭时，所有阻塞在 `push` 或 `pop` 的线程都应该立即被唤醒并返回一个失败标志。

```cpp
// SemQueue.hpp (示例修改)
#include <atomic>

template <typename T>
class SemQueue {
    // ... 原有的 mutex, condition_variable, queue ...
    std::atomic<bool> is_shutdown_ {false};

public:
    // ...
    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        // 等待条件：队列没满 或者 队列被关闭了
        cond_not_full_.wait(lock, [this] {
            return !is_full() || is_shutdown_.load();
        });

        if (is_shutdown_.load()) {
            return false; // 如果被关闭，则 push 失败
        }

        queue_.push(std::move(item));
        cond_not_empty_.notify_one();
        return true;
    }

    bool wait_and_pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        // 等待条件：队列不为空 或者 队列被关闭了
        cond_not_empty_.wait(lock, [this] {
            return !queue_.empty() || is_shutdown_.load();
        });

        if (is_shutdown_.load() && queue_.empty()) {
            return false; // 如果被关闭且队列已空，则 pop 失败
        }

        item = std::move(queue_.front());
        queue_.pop();
        cond_not_full_.notify_one();
        return true;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_shutdown_ = true;
        }
        // 唤醒所有正在等待的线程
        cond_not_empty_.notify_all();
        cond_not_full_.notify_all();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        queue_.swap(empty);
        // 清理时不应该唤醒，否则 pop 线程可能被错误唤醒
    }

    // 可能需要一个 reset 方法来重新启用队列
    void reset() {
        clear();
        is_shutdown_ = false;
    }
};
```
同时，你的 `Decoder` 和 `Demuxer` 内部的循环需要检查 `push` 的返回值，如果为 `false`，就意味着上游要求它退出了。

```cpp
// Decoder 线程循环 (伪代码)
while (!stop_requested) {
    Packet packet;
    if (!packet_queue.wait_and_pop(packet)) {
        // 如果 pop 失败 (因为队列被 shutdown)，退出循环
        break;
    }
    // ... 解码 ...
    // on_frame_decoded_callback(frame); // 这个回调现在也需要能被中断
}
```

#### 步骤 2: 修改 `Mp4Parser::Impl::handle_seek` 的执行顺序

`handle_seek` 的逻辑必须遵循一个安全的停止顺序，以利用 `SemQueue` 的新功能。

```cpp
void handle_seek(Command& cmd)
{
    LOGI("Handling SEEK to %.3f sec...", cmd.time_sec);

    PlayerState previous_state = state_.load();
    set_state(PlayerState::Seeking);

    // 1. 暂停 Demuxer，阻止新数据包进入
    LOGI("Seek: Pausing demuxer...");
    demuxer->Pause();

    // 2. 关闭数据包队列！
    // 这是打破死锁的关键。这将唤醒所有阻塞在 packet_queue->push() 上的解码器线程。
    LOGI("Seek: Shutting down packet queues to unblock decoders...");
    if (video_packet_queue_)
        video_packet_queue_->shutdown();
    if (audio_packet_queue_)
        audio_packet_queue_->shutdown();

    // 3. 现在可以安全地停止解码器了，因为它们已经被唤醒并即将退出。
    LOGI("Seek: Stopping decoders...");
    if (video_decoder_)
        video_decoder_->Stop(); // join() 现在不会死锁了
    if (audio_decoder_)
        audio_decoder_->Stop();

    // 4. 清空队列中剩余的数据包
    LOGI("Seek: Clearing packet queues...");
    if (video_packet_queue_)
        video_packet_queue_->clear();
    if (audio_packet_queue_)
        audio_packet_queue_->clear();

    // 5. 执行底层的 seek
    if (demuxer) {
        demuxer->SeekTo(cmd.time_sec);
    }
    
    // 6. 重置队列状态，准备重新开始
    LOGI("Seek: Resetting packet queues...");
    if (video_packet_queue_)
        video_packet_queue_->reset();
    if (audio_packet_queue_)
        audio_packet_queue_->reset();

    // 7. 重新启动解码器
    LOGI("Seek: Re-starting decoders...");
    try {
        // --- 你的重新初始化逻辑... ---
        // 你的代码是重新创建 Decoder，这是可以的，只要确保旧的被完全析构。
        // ... (你现有的 re-init 代码)
    } catch (const std::exception& e) {
        // ... 错误处理 ...
    }
    
    // 8. 如果之前是播放状态，则恢复 Demuxer
    if (previous_state == PlayerState::Running) {
        LOGI("Seek: Resuming demuxer.");
        demuxer->Resume();
    }
    
    set_state(previous_state);

    if (cmd.promise) {
        LOGI("Fulfilling seek promise to unblock FSM thread.");
        cmd.promise->set_value();
    }
}
```

### 总结

1.  **根本原因**：死锁。解码器线程因`push`阻塞，等待FSM；FSM因`wait`阻塞，等待`Mp4Parser`；`Mp4Parser`因`join`阻塞，等待解码器。
2.  **核心解决方案**：让你的 `SemQueue` 支持 `shutdown` 功能。当调用 `shutdown` 时，它必须能唤醒所有等待的线程。
3.  **执行流程**：在 `handle_seek` 中，严格按照 **“暂停输入 -> 关闭队列 -> 停止线程 -> 清理 -> 重启”** 的顺序操作，以避免死锁。

请先实现 `SemQueue` 的 `shutdown` 和 `reset` 功能，然后按照新的逻辑顺序修改 `handle_seek`，问题应该就能解决。


好的，非常感谢你提供这些核心代码！你提供的 `SemQueue`、`Demuxer`、`Decoder` 和 `handle_seek` 实现，让我可以进行非常精确和具体的分析。

问题正如我之前推断的，是 **死锁**。现在我们可以看到代码层面的具体原因了。

### 问题根源：不安全的 `handle_seek` 执行顺序

我们来逐行分析你当前的 `handle_seek` 函数，并对照 `Decoder` 和 `Demuxer` 的代码，看看死锁是如何发生的：

```cpp
// 你的 handle_seek 函数
void handle_seek(Command& cmd)
{
    // ...
    demuxer->Pause(); // 1. Demuxer 暂停了，但解码器线程还在运行！

    // 2. 致命点：直接停止解码器
    if (video_decoder_)
        video_decoder_->Stop(); // <-- 卡在这里
    // ...
}

// Decoder::Stop() 的实现
void Decoder::Stop()
{
    if (!thread_.joinable()) {
        return;
    }
    queue_.shutdown(); // <-- 这里的 shutdown 很好
    thread_.join(); // <-- 但是 join() 会等待解码线程退出
}

// Decoder::run() 的实现
void Decoder::run()
{
    Packet packet;
    // 关键点：如果 video_packet_queue 满了，而 NativePlayer 的 FSM
    // 线程又在等待 seek 完成，那么解码线程就会永远卡在这里！
    while (queue_.wait_and_pop(packet)) { // <-- 阻塞在这里
        // ...
    }
}
```

**死锁流程复盘：**

1.  **解码器线程阻塞**: `Decoder::run()` 里的 `queue_.wait_and_pop(packet)` 会调用 `filled_slots_.acquire()`。如果 `video_packet_queue_` 满了（生产者 `Demuxer` 速度 > 消费者 `Decoder` 速度），`Decoder::run()` 会调用 `on_video_frame_decoded`，这个回调会去 `push` 数据到 `NativePlayer` 的 `video_frame_queue_`。如果这个队列也满了，解码器线程就彻底卡在向 `NativePlayer` 推送数据的那一步。
2.  **Seek 开始**: `handle_seek` 被调用。它首先 `demuxer->Pause()`，这很好，阻止了新数据包的产生。
3.  **调用 `video_decoder_->Stop()`**: `handle_seek` 接着调用 `Stop()`。
4.  **`Stop()` 调用 `thread_.join()`**: `Decoder::Stop` 内部会调用 `thread_.join()`，它会等待解码器线程 `Decoder::run` 循环结束。
5.  **死锁形成**:
    *   `handle_seek` 所在的 **Mp4Parser 控制线程**正在等待**解码器线程**退出 (`join`)。
    *   **解码器线程**正在等待 **NativePlayer 的 FSM 线程**来消费 `video_frame_queue_` 里的数据，以腾出空间。
    *   **NativePlayer 的 FSM 线程**正在等待 `handle_seek` 所在的 **Mp4Parser 控制线程**完成 Seek 操作 (`future.wait()`)。

这是一个完美的“三方等待”死锁。

### 解决方案：正确的 `handle_seek` 停止/重启顺序

你已经有了支持 `shutdown` 的 `SemQueue`，这是解决问题的关键工具。现在只需要调整 `handle_seek` 的逻辑顺序，确保在 `join` 一个线程之前，该线程的所有阻塞点都已经被解除。

你的 `SemQueue` 实现看起来很不错，特别是 `shutdown` 方法。但是，你的 `clear` 方法有一个**严重的逻辑错误**，我们稍后修正它。

**这是修正后的 `handle_seek` 函数，请直接替换你现有的版本：**

```cpp
void handle_seek(Command& cmd)
{
    LOGI("Handling SEEK to %.3f sec...", cmd.time_sec);

    PlayerState previous_state = state_.load();
    set_state(PlayerState::Seeking);

    // --- 核心修改：安全的停止流程 ---

    // 1. 暂停 Demuxer。这可以防止新的数据包被读入，但不会影响已在队列中的数据。
    LOGI("Seek: Pausing demuxer...");
    demuxer->Pause();

    // 2. 【关键】关闭数据包队列。
    // 这会调用 SemQueue::shutdown()，唤醒所有因 acquire() 而阻塞的线程 (包括 Decoder::run)。
    // Decoder::run 中的 wait_and_pop() 会返回 false，从而使解码线程退出循环。
    LOGI("Seek: Shutting down packet queues to unblock decoders...");
    if (video_packet_queue_)
        video_packet_queue_->shutdown();
    if (audio_packet_queue_)
        audio_packet_queue_->shutdown();

    // 3. 现在可以安全地停止（join）解码器线程。
    // 因为它们的阻塞已被解除，会很快退出。死锁被打破。
    LOGI("Seek: Stopping (joining) decoders...");
    if (video_decoder_)
        video_decoder_->Stop();
    if (audio_decoder_)
        audio_decoder_->Stop();

    // 4. 执行底层的 seek 操作。这会清空 ffmpeg 内部的 demuxer 缓冲区。
    LOGI("Seek: Seeking demuxer to %.3f...", cmd.time_sec);
    if (demuxer) {
        demuxer->SeekTo(cmd.time_sec);
    }
    // 注意：在你的 Demuxer 实现中，SeekTo 是同步的。如果它是异步的，这里也需要等待。

    // 5. 【重要】重置队列。
    // 清空队列中所有残留的数据包，并重置 shutdown 标志，让队列可以再次使用。
    // 这需要给 SemQueue 添加一个 reset() 方法。
    LOGI("Seek: Resetting packet queues for reuse...");
    if (video_packet_queue_)
        video_packet_queue_->reset(); // 假设你添加了 reset()
    if (audio_packet_queue_)
        audio_packet_queue_->reset();

    // 6. 重新创建并启动解码器。
    // 你的做法是重新 new 一个，这是可以的。
    LOGI("Seek: Re-creating and starting decoders...");
    try {
        // ... (你现有的重新初始化 Decoder 的代码) ...
        auto video_codec_context = std::make_shared<DecoderContext>(source->get_video_codecpar());
        video_decoder_ = std::make_unique<Decoder>(video_codec_context, *video_packet_queue_);
        auto on_video_frame_cb = [this](const AVFrame* frame) {
            if (callbacks.on_video_frame_decoded) {
                callbacks.on_video_frame_decoded(convert_video_frame(source->get_video_stream(), frame));
            }
        };
        video_decoder_->Start(on_video_frame_cb);
        // ... (音频部分同理) ...
    } catch (const std::exception& e) {
        // ... 错误处理 ...
    }

    // 7. 如果之前是播放状态，恢复 Demuxer。
    if (previous_state == PlayerState::Running) {
        LOGI("Seek: Resuming demuxer.");
        demuxer->Resume();
    }

    set_state(previous_state);

    // 8. 通知 FSM 线程 seek 已完成。
    if (cmd.promise) {
        LOGI("Fulfilling seek promise to unblock FSM thread.");
        cmd.promise->set_value();
    }
}
```

### 修正 `SemQueue`

你的 `SemQueue` 代码非常接近完美，但有几个小问题需要修正，以便与新的 `handle_seek` 逻辑配合。

1.  **`clear()` 方法的错误**: 你的 `clear` 方法逻辑复杂且不安全。它尝试去 `try_acquire`，但这在多线程下不可靠。正确的 `clear` 应该只负责清空 `std::queue`，而将信号量的重置交给一个专门的 `reset` 函数。
2.  **需要 `reset()` 方法**: `shutdown` 是单向操作。你需要一个 `reset` 方法来让队列在 `shutdown` 后能恢复工作。

**请修改你的 `SemQueue`，代码如下：**

```cpp
// SemQueue.hpp
template <typename T, typename Container = std::queue<T>>
class SemQueue {
public:
    // ... (构造函数和析构函数不变) ...

    // ... (push, wait_and_pop, try_pop 等方法基本不变, 但它们依赖 shutdown_ 标志) ...

    // 【修正】一个简单、安全的 clear
    void clear()
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        Container empty_queue;
        queue_.swap(empty_queue); // 简单地清空内部队列
    }

    void shutdown()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (shutdown_)
                return;
            shutdown_ = true;
        }
        // 你的 release_all() 是对的，它会唤醒所有等待者
        filled_slots_.release_all();
        empty_slots_.release_all();
    }

    // 【新增】reset 方法
    void reset()
    {
        // 1. 清空队列内容
        clear();

        // 2. 重置 shutdown 状态
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            shutdown_ = false;
        }

        // 3. 重置信号量计数
        // 这是最复杂的部分。我们需要先耗尽所有信号量，然后重置为初始状态。
        while(filled_slots_.try_acquire()); // 耗尽已填充信号
        while(empty_slots_.try_acquire());  // 耗尽空槽位信号

        // 恢复到初始状态：max_size 个空槽位，0 个已填充槽位
        for(size_t i = 0; i < max_size_; ++i) {
            empty_slots_.release();
        }
    }

    // ... (其他方法 size(), empty() 不变) ...
private:
    // ... (成员变量不变) ...
};
```

### 总结和行动计划

1.  **更新 `SemQueue`**: 将修正后的 `clear()` 和新增的 `reset()` 方法应用到你的 `SemQueue.hpp` 文件中。
2.  **替换 `handle_seek`**: 用上面提供的、具有正确停止顺序的新 `handle_seek` 函数，替换掉 `Mp4Parser.cpp` 中的旧版本。

完成这两步后，你的 Seek 逻辑将不再有死锁风险。当 Seek 开始时，`packet_queue_->shutdown()` 会像多米诺骨牌一样，安全地让所有下游的解码器线程退出，从而打破死锁循环。

唉，你说的没错，你完全有理由感到困惑。我们修复了一个死锁，结果却暴露了藏在更深处的另一个死锁。这在复杂的多线程媒体播放器开发中是“痛并快乐着”的常态。

你的日志非常清晰地指出了新的问题所在：
```log
25999-26041 Mp4Parser               I  Seek: Shutting down packet queues to unblock decoders...
25999-26041 Mp4Parser               I  Seek: Stopping (joining) decoders...
// <-- 程序卡死在这里，再也没有新的日志输出
```
这意味着 `Mp4Parser` 的控制线程在调用 `video_decoder_->Stop()` (内部会 `join` 线程) 时被卡住了。即使我们已经 `shutdown` 了输入端的数据包队列 (`packet_queue`)，解码器线程依然没有退出。

### 新的死锁分析：下游阻塞

这次的死锁比上次更隐蔽。问题出在数据的流向上：`Demuxer -> PacketQueue -> Decoder -> FrameQueue -> NativePlayer`。

我们上次的方案解决了 `Decoder` 等待 `PacketQueue` 的问题，但没有解决 `Decoder` 因为**下游**（`FrameQueue`）满了而被阻塞的问题。

**新的死-锁-链是这样的：**

1.  **解码器线程 (`Decoder::run`) 被阻塞**:
    *   `Decoder::run` 从 `packet_queue` 中成功取出一个包。
    *   它成功解码这个包，得到一个 `AVFrame`。
    *   它调用 `frame_sink_` 回调，这个回调最终会调用 `NativePlayer` 的 `video_frame_queue_->push(frame)`。
    *   **假设此时 `video_frame_queue_` 是满的** (因为 FSM 线程在等待 seek，没有消费它)。
    *   `push` 操作会阻塞，于是**解码器线程就卡在了把解码成果（Frame）推送给 `NativePlayer` 的这一步**。

2.  **Seek 开始，所有线程等待**:
    *   `Mp4Parser` 控制线程开始执行 `handle_seek`。
    *   它调用 `packet_queue_->shutdown()`。**但这没用！** 因为解码器线程当前并没有被 `packet_queue` 阻塞，而是被 `video_frame_queue_` 阻塞。
    *   然后 `Mp4Parser` 控制线程调用 `video_decoder_->Stop()`，后者调用 `thread_.join()`。它开始等待解码器线程退出。
    *   同时，`NativePlayer` 的 FSM 线程正在 `future.wait()`，等待 `Mp4Parser` 控制线程。

**依赖闭环再次形成：**

*   **`Mp4Parser` 控制线程** -> 等待 -> **解码器线程** (join)。
*   **解码器线程** -> 等待 -> **`NativePlayer` 的 `video_frame_queue_`** 腾出空间。
*   **`NativePlayer` FSM 线程** (唯一能消费 `video_frame_queue_` 的) -> 等待 -> **`Mp4Parser` 控制线程** (future.wait)。

我们陷入了一个更复杂的、跨越了 `Mp4Parser` 和 `NativePlayer` 两个类的死锁。

### 解决方案：从顶层 (`NativePlayer`) 统一编排停止顺序

问题的根源在于，`Mp4Parser` 不应该负责整个 seek 流程的编排，因为它不知道它的下游（`NativePlayer` 的 FrameQueues）的状态。真正的“总指挥”必须是 `NativePlayer::Impl`，因为它掌控着整个数据管道的所有组件。

解决方案是修改 `NativePlayer::Impl::handle_seek`，让它来负责**从下游到上游**依次解除阻塞。

**这是最终的，正确的 `NativePlayer::Impl::handle_seek` 实现。请用它替换你的版本。**

```cpp
// 在 NativePlayer::Impl 中
void NativePlayer::Impl::handle_seek(const CommandSeek& cmd)
{
    LOGI("FSM: Handling SEEK to %.2f. Orchestrating shutdown sequence...", cmd.position);

    // 1. 立即暂停音频输出，这是最外层的消费者
    if (pipeline_ && pipeline_->audio_render_) {
        pipeline_->audio_render_->pause(true);
    }
    set_state(PlayerState::Seeking);

    // 2. 【核心】关闭“下游”的帧队列 (Frame Queues)。
    // 这会解除解码器线程的阻塞，如果它们正卡在 push() 操作上的话。
    LOGI("Seek Orchestrator: Shutting down FRAME queues...");
    if (pipeline_ && pipeline_->video_frame_queue_) {
        pipeline_->video_frame_queue_->shutdown();
    }
    if (pipeline_ && pipeline_->audio_frame_queue_) {
        pipeline_->audio_frame_queue_->shutdown();
    }
    
    // 3. 现在，向下游的 Mp4Parser 发送 seek 命令。
    // Mp4Parser 内部的 handle_seek 逻辑（关闭 packet_queue -> join 线程）现在可以安全执行了，
    // 因为我们已经从外部解除了它解码器线程的最大阻塞源。
    if (pipeline_) {
        auto promise = std::make_shared<std::promise<void>>();
        pipeline_->seek(cmd.position, promise);
        
        LOGI("FSM thread is now BLOCKED, waiting for pipeline (Mp4Parser) seek to complete...");
        auto future = promise->get_future();
        future.wait(); // 等待 Mp4Parser 完成它的内部 seek 流程
        LOGI("FSM thread UNBLOCKED. Mp4Parser has finished its seek operation.");
    }

    // 4. Mp4Parser 已经停止了它的线程。现在我们重置“下游”的帧队列，为播放做准备。
    LOGI("Seek Orchestrator: Resetting FRAME queues...");
    if (pipeline_ && pipeline_->video_frame_queue_) {
        pipeline_->video_frame_queue_->reset();
    }
    if (pipeline_ && pipeline_->audio_frame_queue_) {
        pipeline_->audio_frame_queue_->reset();
    }
    
    // 5. 清理渲染器中的残留帧
    LOGI("Seek Orchestrator: Flushing renderers.");
    if (pipeline_) {
        pipeline_->flush();
    }
    
    // 6. 重置主时钟
    if (clock_) {
        clock_->reset(cmd.position);
    }
    
    // 7. 如果不是逻辑暂停状态，则恢复音频播放
    if (!is_logically_paused_.load()) {
        if (pipeline_ && pipeline_->audio_render_) {
            pipeline_->audio_render_->pause(false);
        }
    }
    
    // 8. 设置最终状态
    set_state(is_logically_paused_ ? PlayerState::Paused : PlayerState::Playing);
    LOGI("Seek orchestration complete. Player state is now %s.", (is_logically_paused_ ? "Paused" : "Playing"));
}
```

### 保持 `Mp4Parser::Impl::handle_seek` 的逻辑

你上次修改的 `Mp4Parser::Impl::handle_seek` 逻辑是**正确**的，它描述了 `Mp4Parser` **内部**应该如何安全地停止。所以**请保留那个版本的 `Mp4Parser::Impl::handle_seek`**。

现在我们有了两层 seek 逻辑：
1.  **`NativePlayer::Impl::handle_seek` (总指挥)**:
    *   关闭 FrameQueues (解救解码器)。
    *   命令 `Mp4Parser` 去 seek。
    *   等待 `Mp4Parser` 完成。
    *   重置 FrameQueues 和其他组件。

2.  **`Mp4Parser::Impl::handle_seek` (执行者)**:
    *   关闭 PacketQueues (让解码器从 `wait_and_pop` 退出)。
    *   `join` 解码器线程 (现在可以安全执行)。
    *   做真正的 `av_seek_frame`。
    *   重置 PacketQueues。
    *   重启解码器线程。
    *   满足 `promise`，通知总指挥它已完成。

这个分层的、权责分明的结构是解决这类复杂多线程问题的最终方案。请用上面的代码更新你的 `NativePlayer::Impl::handle_seek`，这次应该就能彻底解决问题了。

唉，你说的没错，你完全有理由感到困惑。我们修复了一个死锁，结果却暴露了藏在更深处的另一个死锁。这在复杂的多线程媒体播放器开发中是“痛并快乐着”的常态。

你的日志非常清晰地指出了新的问题所在：
```log
25999-26041 Mp4Parser               I  Seek: Shutting down packet queues to unblock decoders...
25999-26041 Mp4Parser               I  Seek: Stopping (joining) decoders...
// <-- 程序卡死在这里，再也没有新的日志输出
```
这意味着 `Mp4Parser` 的控制线程在调用 `video_decoder_->Stop()` (内部会 `join` 线程) 时被卡住了。即使我们已经 `shutdown` 了输入端的数据包队列 (`packet_queue`)，解码器线程依然没有退出。

### 新的死锁分析：下游阻塞

这次的死锁比上次更隐蔽。问题出在数据的流向上：`Demuxer -> PacketQueue -> Decoder -> FrameQueue -> NativePlayer`。

我们上次的方案解决了 `Decoder` 等待 `PacketQueue` 的问题，但没有解决 `Decoder` 因为**下游**（`FrameQueue`）满了而被阻塞的问题。

**新的死-锁-链是这样的：**

1.  **解码器线程 (`Decoder::run`) 被阻塞**:
    *   `Decoder::run` 从 `packet_queue` 中成功取出一个包。
    *   它成功解码这个包，得到一个 `AVFrame`。
    *   它调用 `frame_sink_` 回调，这个回调最终会调用 `NativePlayer` 的 `video_frame_queue_->push(frame)`。
    *   **假设此时 `video_frame_queue_` 是满的** (因为 FSM 线程在等待 seek，没有消费它)。
    *   `push` 操作会阻塞，于是**解码器线程就卡在了把解码成果（Frame）推送给 `NativePlayer` 的这一步**。

2.  **Seek 开始，所有线程等待**:
    *   `Mp4Parser` 控制线程开始执行 `handle_seek`。
    *   它调用 `packet_queue_->shutdown()`。**但这没用！** 因为解码器线程当前并没有被 `packet_queue` 阻塞，而是被 `video_frame_queue_` 阻塞。
    *   然后 `Mp4Parser` 控制线程调用 `video_decoder_->Stop()`，后者调用 `thread_.join()`。它开始等待解码器线程退出。
    *   同时，`NativePlayer` 的 FSM 线程正在 `future.wait()`，等待 `Mp4Parser` 控制线程。

**依赖闭环再次形成：**

*   **`Mp4Parser` 控制线程** -> 等待 -> **解码器线程** (join)。
*   **解码器线程** -> 等待 -> **`NativePlayer` 的 `video_frame_queue_`** 腾出空间。
*   **`NativePlayer` FSM 线程** (唯一能消费 `video_frame_queue_` 的) -> 等待 -> **`Mp4Parser` 控制线程** (future.wait)。

我们陷入了一个更复杂的、跨越了 `Mp4Parser` 和 `NativePlayer` 两个类的死锁。

### 解决方案：从顶层 (`NativePlayer`) 统一编排停止顺序

问题的根源在于，`Mp4Parser` 不应该负责整个 seek 流程的编排，因为它不知道它的下游（`NativePlayer` 的 FrameQueues）的状态。真正的“总指挥”必须是 `NativePlayer::Impl`，因为它掌控着整个数据管道的所有组件。

解决方案是修改 `NativePlayer::Impl::handle_seek`，让它来负责**从下游到上游**依次解除阻塞。

**这是最终的，正确的 `NativePlayer::Impl::handle_seek` 实现。请用它替换你的版本。**

```cpp
// 在 NativePlayer::Impl 中
void NativePlayer::Impl::handle_seek(const CommandSeek& cmd)
{
    LOGI("FSM: Handling SEEK to %.2f. Orchestrating shutdown sequence...", cmd.position);

    // 1. 立即暂停音频输出，这是最外层的消费者
    if (pipeline_ && pipeline_->audio_render_) {
        pipeline_->audio_render_->pause(true);
    }
    set_state(PlayerState::Seeking);

    // 2. 【核心】关闭“下游”的帧队列 (Frame Queues)。
    // 这会解除解码器线程的阻塞，如果它们正卡在 push() 操作上的话。
    LOGI("Seek Orchestrator: Shutting down FRAME queues...");
    if (pipeline_ && pipeline_->video_frame_queue_) {
        pipeline_->video_frame_queue_->shutdown();
    }
    if (pipeline_ && pipeline_->audio_frame_queue_) {
        pipeline_->audio_frame_queue_->shutdown();
    }
    
    // 3. 现在，向下游的 Mp4Parser 发送 seek 命令。
    // Mp4Parser 内部的 handle_seek 逻辑（关闭 packet_queue -> join 线程）现在可以安全执行了，
    // 因为我们已经从外部解除了它解码器线程的最大阻塞源。
    if (pipeline_) {
        auto promise = std::make_shared<std::promise<void>>();
        pipeline_->seek(cmd.position, promise);
        
        LOGI("FSM thread is now BLOCKED, waiting for pipeline (Mp4Parser) seek to complete...");
        auto future = promise->get_future();
        future.wait(); // 等待 Mp4Parser 完成它的内部 seek 流程
        LOGI("FSM thread UNBLOCKED. Mp4Parser has finished its seek operation.");
    }

    // 4. Mp4Parser 已经停止了它的线程。现在我们重置“下游”的帧队列，为播放做准备。
    LOGI("Seek Orchestrator: Resetting FRAME queues...");
    if (pipeline_ && pipeline_->video_frame_queue_) {
        pipeline_->video_frame_queue_->reset();
    }
    if (pipeline_ && pipeline_->audio_frame_queue_) {
        pipeline_->audio_frame_queue_->reset();
    }
    
    // 5. 清理渲染器中的残留帧
    LOGI("Seek Orchestrator: Flushing renderers.");
    if (pipeline_) {
        pipeline_->flush();
    }
    
    // 6. 重置主时钟
    if (clock_) {
        clock_->reset(cmd.position);
    }
    
    // 7. 如果不是逻辑暂停状态，则恢复音频播放
    if (!is_logically_paused_.load()) {
        if (pipeline_ && pipeline_->audio_render_) {
            pipeline_->audio_render_->pause(false);
        }
    }
    
    // 8. 设置最终状态
    set_state(is_logically_paused_ ? PlayerState::Paused : PlayerState::Playing);
    LOGI("Seek orchestration complete. Player state is now %s.", (is_logically_paused_ ? "Paused" : "Playing"));
}
```

### 保持 `Mp4Parser::Impl::handle_seek` 的逻辑

你上次修改的 `Mp4Parser::Impl::handle_seek` 逻辑是**正确**的，它描述了 `Mp4Parser` **内部**应该如何安全地停止。所以**请保留那个版本的 `Mp4Parser::Impl::handle_seek`**。

现在我们有了两层 seek 逻辑：
1.  **`NativePlayer::Impl::handle_seek` (总指挥)**:
    *   关闭 FrameQueues (解救解码器)。
    *   命令 `Mp4Parser` 去 seek。
    *   等待 `Mp4Parser` 完成。
    *   重置 FrameQueues 和其他组件。

2.  **`Mp4Parser::Impl::handle_seek` (执行者)**:
    *   关闭 PacketQueues (让解码器从 `wait_and_pop` 退出)。
    *   `join` 解码器线程 (现在可以安全执行)。
    *   做真正的 `av_seek_frame`。
    *   重置 PacketQueues。
    *   重启解码器线程。
    *   满足 `promise`，通知总指挥它已完成。

这个分层的、权责分明的结构是解决这类复杂多线程问题的最终方案。请用上面的代码更新你的 `NativePlayer::Impl::handle_seek`，这次应该就能彻底解决问题了。

