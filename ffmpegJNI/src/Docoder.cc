#include "Decoder.hpp"
#include "Packet.hpp" // For ffmpeg_utils::Packet

#include <iostream>
#include <stdexcept>

// The actual FFmpeg headers are only needed in the .cpp file.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
}

Decoder::Decoder(const AVCodecParameters* codec_params,
    player_utils::PacketQueue& source_queue,
    FrameSink frame_sink)
    : queue_(source_queue)
    , frame_sink_(std::move(frame_sink))
{
    if (!codec_params) {
        throw std::invalid_argument("Decoder: AVCodecParameters cannot be null.");
    }
    if (!frame_sink_) {
        throw std::invalid_argument("Decoder: FrameSink cannot be null.");
    }

    // 1. 查找解码器
    const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        throw std::runtime_error("Decoder: Failed to find codec.");
    }

    // 2. 分配解码器上下文
    codec_context_ = avcodec_alloc_context3(codec);
    if (!codec_context_) {
        throw std::runtime_error("Decoder: Failed to allocate AVCodecContext.");
    }

    // 3. 将流参数复制到上下文中
    if (avcodec_parameters_to_context(codec_context_, codec_params) < 0) {
        // 如果失败，需要释放已分配的 context
        avcodec_free_context(&codec_context_);
        throw std::runtime_error("Decoder: Failed to copy codec parameters to context.");
    }

    // 4. 打开解码器
    if (avcodec_open2(codec_context_, codec, nullptr) < 0) {
        avcodec_free_context(&codec_context_);
        throw std::runtime_error("Decoder: Failed to open codec.");
    }

    // 5. 分配用于接收解码后数据的帧
    decoded_frame_ = av_frame_alloc();
    if (!decoded_frame_) {
        avcodec_free_context(&codec_context_); // 别忘了清理
        throw std::runtime_error("Decoder: Could not allocate AVFrame.");
    }

    std::cout << "Decoder initialized successfully for " << codec->name << std::endl;
}

Decoder::~Decoder()
{
    // Free all resources in the reverse order of allocation.
    av_frame_free(&decoded_frame_);
    avcodec_free_context(&codec_context_);
    std::cout << "Decoder resources freed." << std::endl;
}

void Decoder::run()
{
    ffmpeg_utils::Packet packet;

    // Main Loop: continue as long as the queue provides packets.
    // wait_and_pop() will block until a packet is available or the queue is shut down.
    while (queue_.wait_and_pop(packet)) {
        // Send the packet to the decoder.
        // A null packet from packet.get() is valid and used for flushing.
        int ret = avcodec_send_packet(codec_context_, packet.get());
        if (ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
            av_strerror(ret, err_buf, sizeof(err_buf));
            std::cerr << "Decoder: Error sending packet to decoder: " << err_buf << std::endl;
            // Depending on the error, we might want to continue or stop.
            // For simplicity, we'll continue trying to receive frames.
        }

        // After sending a packet, we may be able to receive one or more frames.
        receive_and_process_frames();
    }

    // When the loop finishes, the queue is empty and has been shut down.
    // We now must flush the decoder to get any remaining buffered frames.
    std::cout << "Decoder: End of packet stream. Flushing decoder..." << std::endl;
    flush_decoder();
}

int Decoder::receive_and_process_frames()
{
    int ret = 0;
    // Keep trying to receive frames until the decoder says it needs more data (EAGAIN)
    // or has no more frames to give (EOF).
    while (true) {
        ret = avcodec_receive_frame(codec_context_, decoded_frame_);

        if (ret == 0) { // Success, we have a frame
            frame_sink_(decoded_frame_);
            // We must unreference the frame to allow it to be reused by the decoder.
            av_frame_unref(decoded_frame_);
        } else {
            // AVERROR(EAGAIN): Decoder needs more input packets to produce a frame.
            // AVERROR_EOF: Decoder is fully flushed and has no more output frames.
            // Other errors: A decoding error occurred.
            break; // Exit the loop on any non-success code.
        }
    }
    return ret;
}

void Decoder::flush_decoder()
{
    // To enter draining mode, we send a null packet to the decoder.
    int ret = avcodec_send_packet(codec_context_, nullptr);
    if (ret < 0) {
        std::cerr << "Decoder: Error sending flush packet." << std::endl;
        return;
    }

    // Now, we just keep receiving frames until we get the EOF signal.
    while (receive_and_process_frames() != AVERROR_EOF)
        ;

    std::cout << "Decoder: Flushing complete." << std::endl;
}