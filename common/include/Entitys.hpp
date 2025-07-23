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
    int64_t pts;
};
}