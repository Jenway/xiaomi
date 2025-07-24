#pragma once
#include "AudioFrame.hpp"
#include "Entitys.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

struct AVFrame;

namespace mp4parser {

using player_utils::AudioFrame;
using player_utils::VideoFrame;

enum class PlayerState : uint8_t {
    Stopped,
    Running,
    Paused,
    Finished,
    Error,
};

struct Config {
    std::string file_path;
    int max_packet_queue_size = 300;
    int max_audio_packet_queue_size = 600;
};

struct Callbacks {
    std::function<void(std::shared_ptr<VideoFrame>)> on_video_frame_decoded;
    std::function<void(std::shared_ptr<AudioFrame>)> on_audio_frame_decoded;
    std::function<void(PlayerState& state)> on_state_changed;
    std::function<void(const std::string& msg)> on_error;
    std::function<void()> on_playback_finished;
};

class Mp4Parser {
public:
    static std::unique_ptr<Mp4Parser> create(const Config& config, const Callbacks& callbacks);
    void start(); // 启动解码流程（开启两个线程）
    void pause(); // 请求暂停（阻塞 decoder 线程）
    void resume(); // 恢复运行
    void stop(); // 停止线程，释放资源
    void seek(double time_sec);

    double get_duration();
    [[nodiscard]] player_utils::AudioParams getAudioParams() const;
    [[nodiscard]] PlayerState get_state() const;

    ~Mp4Parser();

private:
    Mp4Parser() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

void log_info(const std::string& msg);
std::string describe_frame_info(const AVFrame* frame);
const char* state_to_string(PlayerState state);

} // namespace mp4parser
