#include "MediaSource.hpp"
#include <stdexcept>

bool MediaSource::open(const std::string& filename)
{
    if (avformat_open_input(&fmt_ctx_, filename.c_str(), nullptr, nullptr) < 0)
        return false;

    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0)
        return false;

    video_stream_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio_stream_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    return video_stream_index_ >= 0 || audio_stream_index_ >= 0;
}

MediaSource::~MediaSource()
{
    if (fmt_ctx_ != nullptr) {
        avformat_close_input(&fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
    avformat_network_deinit();
}
