#include "MediaSource.hpp"
#include <stdexcept>

bool MediaSource::open(const std::string& filename)
{
    avformat_network_init();
    int ret = avformat_open_input(&fmt_ctx_, filename.c_str(), nullptr, nullptr);
    if (ret != 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        throw std::runtime_error("Failed to open input file: " + filename + " (" + errbuf + ")");
    }
    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        throw std::runtime_error("Failed to find stream info");
    }

    video_stream_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index_ < 0) {
        throw std::runtime_error("No video stream found");
    }
    return true;
}

MediaSource::~MediaSource()
{
    if (fmt_ctx_ != nullptr) {
        avformat_close_input(&fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
    avformat_network_deinit();
}
