// FrameProcessor.hpp
#pragma once

#include "Entitys.hpp"
extern "C" {
#include <libavutil/frame.h>
}
#include <memory>

struct AVStream;

namespace mp4parser {

std::shared_ptr<player_utils::VideoFrame> convert_frame(AVStream* stream, const AVFrame* frame);

} // namespace mp4parser
