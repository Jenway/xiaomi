#include "Decoder.hpp"
#include "Packet.hpp"

#include <iostream>
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
}

using ffmpeg_utils::Packet;
using player_utils::SemQueue;

Decoder::Decoder(std::shared_ptr<DecoderContext> ctx,
    SemQueue<Packet>& source_queue,
    FrameSink frame_sink)
    : queue_(source_queue)
    , ctx_(std::move(ctx))
    , frame_sink_(std::move(frame_sink))
{
    if (!ctx_ || (ctx_->get() == nullptr)) {
        throw std::invalid_argument("Decoder: Invalid DecoderContext.");
    }
    if (!frame_sink_) {
        throw std::invalid_argument("Decoder: FrameSink cannot be null.");
    }

    decoded_frame_ = av_frame_alloc();
    std::cout << "Decoder initialized with existing DecoderContext." << '\n';
}

Decoder::~Decoder()
{
    av_frame_free(&decoded_frame_);
    std::cout << "Decoder resources freed." << '\n';
}

void Decoder::run()
{
    ffmpeg_utils::Packet packet;

    while (queue_.wait_and_pop(packet)) {
        // send -> 指把 packet send 到 codec 的内部队列
        int ret = avcodec_send_packet(ctx_->get(), packet.get());
        if (ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
            av_strerror(ret, err_buf, sizeof(err_buf));
            std::cerr << "Decoder: Error sending packet to decoder: " << err_buf << '\n';
        }
        receive_and_process_frames();
    }

    std::cout << "Decoder: End of packet stream. Flushing decoder..." << '\n';
    flush();
}

int Decoder::receive_and_process_frames()
{
    int ret = 0;

    while (true) {
        // 从 codec 队列中拿解码好的 frame
        ret = avcodec_receive_frame(ctx_->get(), decoded_frame_);

        if (ret == 0) {
            frame_sink_(decoded_frame_);
            av_frame_unref(decoded_frame_);
        } else {
            break;
        }
    }
    return ret;
}

// 帮助清空 codec 内部缓冲区
void Decoder::flush()
{
    int ret = avcodec_send_packet(ctx_->get(), nullptr);
    if (ret < 0) {
        std::cerr << "Decoder: Error sending flush packet." << '\n';
        return;
    }
    while (receive_and_process_frames() != AVERROR_EOF)
        ;

    std::cout << "Decoder: Flushing complete." << '\n';
}