#pragma once
#include "DecoderContext.hpp"
#include "PacketQueue.hpp"
#include <functional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class Decoder {
public:
    using FrameSink = std::function<void(const AVFrame*)>;

    Decoder(std::shared_ptr<DecoderContext> ctx,
        player_utils::PacketQueue& source_queue,
        FrameSink frame_sink);

    ~Decoder();

    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    void run();

private:
    int receive_and_process_frames();
    void flush_decoder();

    // --- Member Variables ---
    player_utils::PacketQueue& queue_;
    std::shared_ptr<DecoderContext> ctx_ = nullptr;
    AVFrame* decoded_frame_ = nullptr;
    FrameSink frame_sink_;
};
