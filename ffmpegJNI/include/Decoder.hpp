#pragma once
#include "DecoderContext.hpp"
#include "Packet.hpp"
#include "SemQueue.hpp"
#include <functional>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class Decoder {
public:
    using FrameSink = std::function<void(const AVFrame*)>;

    Decoder(std::shared_ptr<DecoderContext> ctx,
        player_utils::SemQueue<ffmpeg_utils::Packet>& source_queue,
        FrameSink frame_sink);

    ~Decoder();

    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    void run();

private:
    int receive_and_process_frames();
    void flush_decoder();

    // --- Member Variables ---
    player_utils::SemQueue<ffmpeg_utils::Packet>& queue_;
    std::shared_ptr<DecoderContext> ctx_ = nullptr;
    AVFrame* decoded_frame_ = nullptr;
    FrameSink frame_sink_;
};
