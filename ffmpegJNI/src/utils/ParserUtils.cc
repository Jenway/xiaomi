#include "BinarySemaphore.hpp"
#include "Decoder.hpp"
#include "DecoderContext.hpp"
#include "Demuxer.hpp"
#include "MediaSource.hpp"
#include "Mp4Parser.hpp"
#include "SemQueue.hpp"

#include <android/log.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

// FFmpeg headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

// Define logging macros for convenience
#define LOG_TAG "Mp4Parser"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using ffmpeg_utils::Packet;
using player_utils::SemQueue;

namespace mp4parser {

const char* state_to_string(PlayerState state)
{
    switch (state) {
    case PlayerState::Stopped:
        return "Stopped";
    case PlayerState::Running:
        return "Running";
    case PlayerState::Paused:
        return "Paused";
    case PlayerState::Finished:
        return "Finished";
    case PlayerState::Error:
        return "Error";
    default:
        return "Unknown";
    }
}

} // namespace mp4parser