#include "Mp4Parser/FrameProcessor.hpp"
#include <android/log.h>
#include <cstring>

extern "C" {
#include "libavformat/avformat.h"
#include <libavutil/imgutils.h>
}

#define LOG_TAG "FrameProcessor"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace mp4parser {

std::shared_ptr<player_utils::VideoFrame> convert_frame(AVStream* stream, const AVFrame* frame)
{
    if ((frame == nullptr) || (frame->data[0] == nullptr)) {
        LOGE("Invalid AVFrame: data[0] is null.");
        return nullptr;
    }

    auto out = std::make_shared<player_utils::VideoFrame>();
    out->width = frame->width;
    out->height = frame->height;
    out->format = static_cast<AVPixelFormat>(frame->format);

    // 转换 PTS 时间戳
    if (frame->pts != AV_NOPTS_VALUE) {
        out->pts = static_cast<double>(frame->pts) * av_q2d(stream->time_base);
        LOGD("Converted frame: %dx%d, PTS=%.3f",
            frame->width, frame->height, out->pts);
    } else {
        out->pts = 0.0;
        LOGD("Converted frame without PTS.");
    }

    // 预分配空间
    int total_size = 0;
    for (int i = 0; i < AV_NUM_DATA_POINTERS && (frame->data[i] != nullptr); ++i) {
        int plane_height = (i == 0) ? frame->height : (frame->height + 1) / 2;
        total_size += frame->linesize[i] * plane_height;
    }

    if (total_size <= 0) {
        LOGE("Total frame size invalid: %d", total_size);
        return nullptr;
    }

    out->data.resize(total_size);
    uint8_t* dst = out->data.data();

    // 深拷贝
    for (int i = 0; i < AV_NUM_DATA_POINTERS && frame->data[i]; ++i) {
        int plane_height = (i == 0) ? frame->height : (frame->height + 1) / 2;
        int bytes = frame->linesize[i] * plane_height;
        memcpy(dst, frame->data[i], bytes);
        out->linesize[i] = frame->linesize[i];
        dst += bytes;
    }
    LOGD("Frame data copied, total size: %d bytes.", total_size);

    return out;
}

} // namespace mp4parser
