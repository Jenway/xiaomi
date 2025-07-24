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

struct AudioParams {
    int32_t sample_rate;
    int32_t channel_count;
};

enum class PlayerState {
    None,
    Playing,
    Paused,
    End,
    Seeking,
    Error
};

inline const char* state_to_string(PlayerState state)
{
    switch (state) {
    case PlayerState::End:
        return "End";
    case PlayerState::None:
        return "None";
    case PlayerState::Playing:
        return "Playing";
    case PlayerState::Paused:
        return "Paused";
    case PlayerState::Seeking:
        return "Seeking";
    case PlayerState::Error:
        return "Error";
    default:
        return "Unknown";
    }
}

} // namespace player_utils