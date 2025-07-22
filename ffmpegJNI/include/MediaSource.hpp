#pragma once

#include <stdexcept>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

class MediaSource {
public:
    MediaSource(const std::string& filename)
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
    }

    ~MediaSource()
    {
        if (fmt_ctx_) {
            avformat_close_input(&fmt_ctx_);
            fmt_ctx_ = nullptr;
        }
        avformat_network_deinit();
    }

    AVFormatContext* get_format_context() const { return fmt_ctx_; }
    int get_video_stream_index() const { return video_stream_index_; }
    AVStream* get_video_stream() const { return fmt_ctx_->streams[video_stream_index_]; }
    AVCodecParameters* get_video_codecpar() const { return get_video_stream()->codecpar; }

private:
    AVFormatContext* fmt_ctx_ = nullptr;
    int video_stream_index_ = -1;
};
