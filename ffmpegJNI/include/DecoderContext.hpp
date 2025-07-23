#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class DecoderContext {
public:
    explicit DecoderContext(const AVCodecParameters* codec_params);
    ~DecoderContext();
    DecoderContext(const DecoderContext&) = delete;
    DecoderContext& operator=(const DecoderContext&) = delete;
    DecoderContext(DecoderContext&& other) noexcept;
    DecoderContext& operator=(DecoderContext&& other) noexcept;

    // Getter
    [[nodiscard]] AVCodecContext* get() const { return codec_ctx_; }
    [[nodiscard]] int width() const { return (codec_ctx_ != nullptr) ? codec_ctx_->width : 0; }
    [[nodiscard]] int height() const { return (codec_ctx_ != nullptr) ? codec_ctx_->height : 0; }

private:
    AVCodecContext* codec_ctx_ = nullptr;
};
