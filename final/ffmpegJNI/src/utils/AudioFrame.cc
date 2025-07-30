#include "AudioFrame.hpp"
extern "C" {
#include "libavutil/mem.h"
}

namespace player_utils {
AudioFrame::~AudioFrame()
{
    if (interleaved_pcm != nullptr) {
        av_free(interleaved_pcm);
        interleaved_pcm = nullptr;
    }
}

}
