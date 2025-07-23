#include "Decoder.hpp"
#include "Packet.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
}

using ffmpeg_utils::Packet;
using player_utils::SemQueue;
using std::shared_ptr;

Decoder::Decoder(shared_ptr<DecoderContext> ctx,
    SemQueue<Packet>& source_queue)
    : queue_(source_queue)
    , ctx_(std::move(ctx))
{
    if (!ctx_ || (ctx_->get() == nullptr)) {
        throw std::invalid_argument("Decoder: Invalid DecoderContext.");
    }

    decoded_frame_ = av_frame_alloc();
    if (decoded_frame_ == nullptr) {
        throw std::runtime_error("Decoder: Failed to allocate frame.");
    }
    std::cout << "Decoder initialized with existing DecoderContext." << '\n';
}

Decoder::~Decoder()
{
    Stop();
    if (decoded_frame_ != nullptr) {
        av_frame_free(&decoded_frame_);
    }
    std::cout << "Decoder destroyed." << '\n';
}

void Decoder::Start(FrameSink frame_sink)
{
    if (thread_.joinable())
        return; // Already started
    stop_requested_ = false;
    frame_sink_ = std::move(frame_sink);
    if (!frame_sink_) {
        throw std::invalid_argument("Decoder: FrameSink cannot be null.");
    }
    thread_ = std::thread(&Decoder::run, this);
}

void Decoder::Stop()
{
    stop_requested_ = true;
    queue_.shutdown(); // 解码线程可能在等队列
    if (thread_.joinable())
        thread_.join();
}

void Decoder::run()
{
    Packet packet;

    while (queue_.wait_and_pop(packet)) {
        if (packet.isFlush()) {
            std::cout << "Decoder: Received flush packet. Flushing codec..." << '\n';
            flush();
            continue;
        }

        // send -> 指把 packet send 到 codec 的内部队列
        int ret = avcodec_send_packet(ctx_->get(), packet.get());
        if (ret == AVERROR(EAGAIN)) {
            receive_and_process_frames();
            ret = avcodec_send_packet(ctx_->get(), packet.get());
        }
        if (ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
            av_strerror(ret, err_buf, sizeof(err_buf));
            std::cerr << "Decoder: send_packet failed: " << err_buf << '\n';
            continue;
        }
        receive_and_process_frames();
    }

    std::cout << "Decoder: End of packet stream. Flushing decoder for EOF..." << '\n';
    flush_eof();
}

int Decoder::receive_and_process_frames()
{
    int ret = 0;

    while (!stop_requested_) {
        ret = avcodec_receive_frame(ctx_->get(), decoded_frame_);
        if (ret == 0) {
            if (frame_sink_)
                frame_sink_(decoded_frame_);
            av_frame_unref(decoded_frame_);
        } else {
            break;
        }
    }

    return ret;
}

void Decoder::flush()
{
    avcodec_flush_buffers(ctx_->get());
}

void Decoder::flush_eof()
{
    avcodec_send_packet(ctx_->get(), nullptr);

    while (!stop_requested_) {
        int ret = receive_and_process_frames();
        if (ret == AVERROR_EOF || ret < 0)
            break;
    }
}