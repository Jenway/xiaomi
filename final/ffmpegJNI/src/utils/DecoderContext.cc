#include "DecoderContext.hpp"
#include <stdexcept>

DecoderContext::DecoderContext(const AVCodecParameters* codec_params)
{
    const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
    if (codec == nullptr)
        throw std::runtime_error("No decoder found");

    codec_ctx_ = avcodec_alloc_context3(codec);
    if (codec_ctx_ == nullptr)
        throw std::runtime_error("Could not allocate AVCodecContext");

    if (avcodec_parameters_to_context(codec_ctx_, codec_params) < 0 || avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
        avcodec_free_context(&codec_ctx_);
        throw std::runtime_error("Could not initialize codec context");
    }

    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
        avcodec_free_context(&codec_ctx_);
        throw std::runtime_error("Decoder: Failed to open codec.");
    }
}

DecoderContext::~DecoderContext()
{
    avcodec_free_context(&codec_ctx_);
}

DecoderContext::DecoderContext(DecoderContext&& other) noexcept
    : codec_ctx_(other.codec_ctx_)
{
    other.codec_ctx_ = nullptr;
}

DecoderContext& DecoderContext::operator=(DecoderContext&& other) noexcept
{
    if (this != &other) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = other.codec_ctx_;
        other.codec_ctx_ = nullptr;
    }
    return *this;
}
