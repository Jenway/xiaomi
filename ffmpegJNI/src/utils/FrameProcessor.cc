#include "Mp4Parser/FrameProcessor.hpp"
#include "Entitys.hpp"
#include <android/log.h>
#include <cstring>

extern "C" {
#include "libavformat/avformat.h"
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}

#define LOG_TAG "FrameProcessor"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace mp4parser {
using player_utils::AudioFrame;

std::shared_ptr<player_utils::VideoFrame> convert_video_frame(AVStream* stream, const AVFrame* frame)
{
    if (!frame || !frame->data[0]) {
        LOGE("Invalid AVFrame: data[0] is null.");
        return nullptr;
    }

    auto out = std::make_shared<player_utils::VideoFrame>();
    out->width = frame->width;
    out->height = frame->height;
    out->format = static_cast<AVPixelFormat>(frame->format);

    if (frame->pts != AV_NOPTS_VALUE) {
        out->pts = static_cast<double>(frame->pts) * av_q2d(stream->time_base);
        LOGD("Converted video frame: %dx%d, PTS=%.3f", frame->width, frame->height, out->pts);
    } else {
        out->pts = 0.0;
        LOGD("Converted video frame without PTS.");
    }

    // 预估数据大小
    int total_size = 0;
    for (int i = 0; i < AV_NUM_DATA_POINTERS && frame->data[i]; ++i) {
        int plane_height = (i == 0) ? frame->height : (frame->height + 1) / 2;
        total_size += frame->linesize[i] * plane_height;
    }

    if (total_size <= 0) {
        LOGE("Invalid total video frame size: %d", total_size);
        return nullptr;
    }

    out->data.resize(total_size);
    uint8_t* dst = out->data.data();

    for (int i = 0; i < AV_NUM_DATA_POINTERS && frame->data[i]; ++i) {
        int plane_height = (i == 0) ? frame->height : (frame->height + 1) / 2;
        int bytes = frame->linesize[i] * plane_height;
        memcpy(dst, frame->data[i], bytes);
        out->linesize[i] = frame->linesize[i];
        dst += bytes;
    }

    LOGD("Video frame data copied, total size: %d bytes.", total_size);
    return out;
}

std::shared_ptr<AudioFrame> convert_audio_frame(AVStream* /*stream*/, const AVFrame* frame)
{
    auto audio_frame = std::make_shared<AudioFrame>();

    audio_frame->nb_samples = frame->nb_samples;
    audio_frame->sample_rate = frame->sample_rate;
    audio_frame->channels = frame->ch_layout.nb_channels;

    AVChannelLayout in_layout = frame->ch_layout;
    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, audio_frame->channels); // stereo, etc.

    SwrContext* swr_ctx = nullptr;
    int ret = swr_alloc_set_opts2(
        &swr_ctx,
        &out_layout,
        AV_SAMPLE_FMT_S16,
        audio_frame->sample_rate,
        &in_layout,
        (AVSampleFormat)frame->format,
        frame->sample_rate,
        0,
        nullptr);
    if (ret < 0 || swr_init(swr_ctx) < 0) {
        LOGE("swr_alloc_set_opts2 or swr_init failed.");
        return nullptr;
    }

    int dst_nb_samples = av_rescale_rnd(
        swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples,
        frame->sample_rate, frame->sample_rate, AV_ROUND_UP);

    int out_buf_size = av_samples_get_buffer_size(
        nullptr, audio_frame->channels, dst_nb_samples, AV_SAMPLE_FMT_S16, 1);
    uint8_t* out_buf = (uint8_t*)av_malloc(out_buf_size);

    uint8_t* out[] = { out_buf };
    int samples_converted = swr_convert(
        swr_ctx,
        out, dst_nb_samples,
        (const uint8_t**)frame->extended_data,
        frame->nb_samples);

    audio_frame->interleaved_pcm = out_buf;
    audio_frame->interleaved_size = samples_converted * audio_frame->channels * sizeof(int16_t);

    // 清理
    swr_free(&swr_ctx);
    av_channel_layout_uninit(&out_layout); // in_layout 是引用 frame 的，不需手动释放

    return audio_frame;
}

} // namespace mp4parser
