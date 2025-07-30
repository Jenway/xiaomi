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
#include <thread>

class Decoder {
public:
    using FrameSink = std::function<bool(const AVFrame*)>;

    Decoder(std::shared_ptr<DecoderContext> ctx,
        player_utils::SemQueue<ffmpeg_utils::Packet>& source_queue);
    ~Decoder();
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    void flush();
    void Start(FrameSink frame_sink);
    void Stop();
    void run();

private:
    void receive_all_available_frames();
    void flush_eof();

    player_utils::SemQueue<ffmpeg_utils::Packet>& queue_;
    std::shared_ptr<DecoderContext> ctx_ = nullptr;
    AVFrame* decoded_frame_ = nullptr;
    FrameSink frame_sink_;

    int64_t last_packet_pts_ = AV_NOPTS_VALUE;

    std::thread thread_;
};
