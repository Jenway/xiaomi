#pragma once

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

class MediaSource {
public:
    explicit MediaSource(const std::string& filename);
    ~MediaSource();

    [[nodiscard]] AVFormatContext* get_format_context() const { return fmt_ctx_; }
    [[nodiscard]] int get_video_stream_index() const { return video_stream_index_; }
    [[nodiscard]] AVStream* get_video_stream() const { return fmt_ctx_->streams[video_stream_index_]; }
    [[nodiscard]] AVCodecParameters* get_video_codecpar() const { return get_video_stream()->codecpar; }

private:
    AVFormatContext* fmt_ctx_ = nullptr;
    int video_stream_index_ = -1;
};
