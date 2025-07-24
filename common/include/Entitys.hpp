#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace player_utils {

struct VideoFrame {
    int width;
    int height;
    int format;
    std::vector<uint8_t> data;
    std::array<int, 8> linesize;
    double pts;
};

struct AudioFrame {
    uint8_t* data[8] = { nullptr };
    int linesize[8] = { 0 };

    int nb_samples = 0;
    int channels = 2;
    int sample_rate = 44100;

    double pts = 0.0;
    double duration = 0.0;
};

enum class PlayerState {
    None,
    End,
    Idle, // 初始状态，尚未 start
    Preparing, // 正在初始化资源
    Ready, // 资源准备完成
    Playing, // 正在播放
    Paused, // 已暂停
    Seeking, // 正在 seek
    Stopping, // 正在停止中
    Stopped, // 已停止
    Error // 错误状态
};

inline const char* state_to_string(PlayerState state)
{
    switch (state) {
    case PlayerState::End:
        return "End";
    case PlayerState::None:
        return "None";
    case PlayerState::Idle:
        return "Idle";
    case PlayerState::Preparing:
        return "Preparing";
    case PlayerState::Ready:
        return "Ready";
    case PlayerState::Playing:
        return "Playing";
    case PlayerState::Paused:
        return "Paused";
    case PlayerState::Seeking:
        return "Seeking";
    case PlayerState::Stopping:
        return "Stopping";
    case PlayerState::Stopped:
        return "Stopped";
    case PlayerState::Error:
        return "Error";
    default:
        return "Unknown";
    }
}

} // namespace player_utils