#include "Decoder.hpp"
#include "Packet.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
}
#include <android/log.h>

#define LOG_TAG "Mp4Parser_Decoder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

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
    // 如果线程已经在运行，则不执行任何操作
    if (thread_.joinable()) {
        LOGW("Decoder::Start called but it is already running.");
        return;
    }
    if (!frame_sink) {
        throw std::invalid_argument("Decoder: FrameSink cannot be null.");
    }
    frame_sink_ = std::move(frame_sink);
    thread_ = std::thread(&Decoder::run, this);
}

void Decoder::Stop()
{
    if (!thread_.joinable()) {
        return;
    }

    queue_.shutdown();

    thread_.join();
    LOGI("Decoder thread has been stopped and joined.");
}

void Decoder::run()
{
    Packet packet;

    while (queue_.wait_and_pop(packet)) {
        if (packet.isFlush()) {
            LOGI("Decoder: Received flush packet. Flushing codec...");
            flush();
            continue;
        }

        if ((packet.get() != nullptr) && packet.get()->pts != AV_NOPTS_VALUE) {
            last_packet_pts_ = packet.get()->pts;
        }

        // send -> 指把 packet send 到 codec 的内部队列
        int ret = avcodec_send_packet(ctx_->get(), packet.get());
        if (ret < 0) {
            LOGE("Decoder: avcodec_send_packet failed: %s", av_err2str(ret));
            // 发送失败后，重置我们保存的pts，因为它没有被消费
            last_packet_pts_ = AV_NOPTS_VALUE;
            continue;
        }

        receive_all_available_frames();
    }

    LOGI("Decoder: End of packet stream. Flushing final frames (EOF)...");
    // 发送一个空包以触发EOF
    avcodec_send_packet(ctx_->get(), nullptr);
    receive_all_available_frames();

    LOGI("Decoder thread finished cleanly.");
}

void Decoder::receive_all_available_frames()
{
    while (true) {
        int ret = avcodec_receive_frame(ctx_->get(), decoded_frame_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            LOGE("Decoder: avcodec_receive_frame failed: %s", av_err2str(ret));
            break;
        }

        if (frame_sink_) {
            frame_sink_(decoded_frame_);
        }

        // 检查解码出的帧是否有有效的PTS。
        // 如果没有，或者它的PTS看起来像一个“陈旧”的值（这很难判断，所以我们主要检查AV_NOPTS_VALUE），
        // 我们就使用我们从上一个数据包中保存的PTS。
        if (decoded_frame_->pts == AV_NOPTS_VALUE) {
            if (last_packet_pts_ != AV_NOPTS_VALUE) {
                LOGD("Frame has no PTS, using last packet PTS: %ld", last_packet_pts_);
                decoded_frame_->pts = last_packet_pts_;
                // 一个 packet 的 PTS 只应该被用于它解码出的第一帧。
                // 后续由该 packet 解码出的 B 帧等的 PTS 会由 FFmpeg 自动计算。
                // 为了避免将一个 PTS 应用于多个帧，我们用完就重置它。
                last_packet_pts_ = AV_NOPTS_VALUE;
            }
        }

        av_frame_unref(decoded_frame_);
    }
}

void Decoder::flush()
{
    if (ctx_ && (ctx_->get() != nullptr)) {
        LOGI("Flushing decoder buffers...");
        avcodec_flush_buffers(ctx_->get());
        last_packet_pts_ = AV_NOPTS_VALUE;
        LOGI("Decoder buffers flushed.");
    }
}
