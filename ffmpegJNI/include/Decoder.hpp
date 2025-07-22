#pragma once
#include "PacketQueue.hpp"
#include <functional>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class Decoder {
public:
    using FrameSink = std::function<void(const AVFrame*)>;

    Decoder(const AVCodecParameters* codec_params,
        player_utils::PacketQueue& source_queue,
        FrameSink frame_sink);

    ~Decoder();

    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    int get_width() const { return codec_context_ ? codec_context_->width : 0; }
    int get_height() const { return codec_context_ ? codec_context_->height : 0; }

    void run();

private:
    int receive_and_process_frames();
    void flush_decoder();

    // --- Member Variables ---

    player_utils::PacketQueue& queue_;
    AVCodecContext* codec_context_ = nullptr;
    AVFrame* decoded_frame_ = nullptr;
    FrameSink frame_sink_;
};
