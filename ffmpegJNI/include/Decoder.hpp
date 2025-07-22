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

    /**
     * @brief Constructs and initializes the Decoder from codec parameters.
     *
     * @param codec_params Parameters of the stream to be decoded. The Decoder
     * will find the appropriate codec and create its own context.
     * @param source_queue The thread-safe queue from which to pull encoded packets.
     * @param frame_sink A callable object to be invoked for each decoded frame.
     */
    Decoder(const AVCodecParameters* codec_params,
        player_utils::PacketQueue& source_queue,
        FrameSink frame_sink);

    ~Decoder();

    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    int get_width() const { return codec_context_ ? codec_context_->width : 0; }
    int get_height() const { return codec_context_ ? codec_context_->height : 0; }

    /**
     * @brief Starts the decoding loop.
     *
     * This method will block until the source_queue is shut down and all packets
     * have been fully processed and flushed from the decoder.
     * This is the main entry point and should be run in its own thread.
     */
    void run();

private:
    /**
     * @brief Attempts to receive all currently available frames from the decoder.
     *
     * For each successfully received frame, it calls the frame_sink_.
     * @return The result from the last avcodec_receive_frame call. Typically 0 on success,
     *         AVERROR(EAGAIN) if more packets are needed, or AVERROR_EOF if the
     *         stream is finished.
     */
    int receive_and_process_frames();

    /**
     * @brief Flushes the decoder to retrieve any buffered frames at the end of the stream.
     */
    void flush_decoder();

    // --- Member Variables ---

    player_utils::PacketQueue& queue_; // The source of encoded packets (we don't own it).
    AVCodecContext* codec_context_ = nullptr; // The FFmpeg decoder context (we own this).
    AVFrame* decoded_frame_ = nullptr; // A reusable frame to receive decoded data (we own this).
    FrameSink frame_sink_; // The destination for our decoded frames.
};
