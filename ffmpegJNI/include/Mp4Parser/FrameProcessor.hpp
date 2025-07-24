// FrameProcessor.hpp
#pragma once

#include "Entitys.hpp"
#include "libavcodec/avcodec.h"
extern "C" {
#include <libavutil/frame.h>
}
#include <memory>

struct AVStream;

namespace mp4parser {

std::shared_ptr<player_utils::VideoFrame> convert_video_frame(AVStream* stream, const AVFrame* frame);
std::shared_ptr<player_utils::AudioFrame> convert_audio_frame(AVStream* stream, const AVFrame* frame);

} // namespace mp4parser
