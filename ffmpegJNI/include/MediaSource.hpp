#pragma once

#include "Entitys.hpp"
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
}

class MediaSource {
public:
    MediaSource() = default;
    ~MediaSource();

    bool open(const std::string& filename);

    [[nodiscard]] AVFormatContext* get_format_context() const { return fmt_ctx_; }

    // 视频
    [[nodiscard]] int get_video_stream_index() const { return video_stream_index_; }
    [[nodiscard]] AVStream* get_video_stream() const { return fmt_ctx_->streams[video_stream_index_]; }
    [[nodiscard]] AVCodecParameters* get_video_codecpar() const { return get_video_stream()->codecpar; }

    // 音频
    [[nodiscard]] int get_audio_stream_index() const { return audio_stream_index_; }
    [[nodiscard]] AVStream* get_audio_stream() const { return fmt_ctx_->streams[audio_stream_index_]; }
    [[nodiscard]] AVCodecParameters* get_audio_codecpar() const { return get_audio_stream()->codecpar; }
    player_utils::AudioParams get_audio_params() const
    {
        if ((fmt_ctx_ != nullptr) && audio_stream_index_ >= 0) {
            AVCodecParameters* params = fmt_ctx_->streams[audio_stream_index_]->codecpar;
            if (params) {
                return {
                    params->sample_rate,
                    params->ch_layout.nb_channels
                };
            }
        }
        // If acquisition fails, return a clear invalid value
        return { -1, -1 };
    }

    bool has_audio_stream() const { return audio_stream_index_ != -1; };

private:
    AVFormatContext* fmt_ctx_ = nullptr;
    int video_stream_index_ = -1;
    int audio_stream_index_ = -1;
};
