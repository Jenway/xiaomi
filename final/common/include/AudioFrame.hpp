#pragma once

#include <cstdint>
namespace player_utils {
struct AudioFrame {
    int nb_samples {};
    int sample_rate {};
    int channels {};
    double pts {};
    double duration {};

    uint8_t* interleaved_pcm = nullptr;
    int interleaved_size = 0;
    ~AudioFrame();
};

} // namespace player_utils
