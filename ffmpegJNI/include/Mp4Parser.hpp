#pragma once

#include <functional>
#include <memory>
#include <string>

struct AVFrame;

namespace mp4parser {

enum class PlayerState {
    Stopped,
    Running,
    Paused,
    Finished,
    Error,
};

struct VideoFrame {
    int width;
    int height;
    int format;
    std::vector<uint8_t> data;
    int linesize[8];
    int64_t pts;
};

std::shared_ptr<VideoFrame> convert_frame(const AVFrame* frame);

struct Config {
    std::string file_path;
    int max_packet_queue_size = 300;
};

struct Callbacks {
    std::function<void(std::shared_ptr<VideoFrame>)> on_frame_decoded;
    std::function<void(PlayerState& state)> on_state_changed;
    std::function<void(const std::string& msg)> on_error;
};

class Mp4Parser {
public:
    static std::unique_ptr<Mp4Parser> create(const Config& config, const Callbacks& callbacks);

    void start(); // 启动解码流程（开启两个线程）
    void pause(); // 请求暂停（阻塞 decoder 线程）
    void resume(); // 恢复运行
    void stop(); // 停止线程，释放资源
    [[nodiscard]] PlayerState get_state() const;

    ~Mp4Parser();

private:
    Mp4Parser() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

void log_info(const std::string& msg);
std::string describe_frame_info(const AVFrame* frame);

} // namespace mp4parser
