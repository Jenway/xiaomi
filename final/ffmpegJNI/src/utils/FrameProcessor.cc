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
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

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

    // LOGD("Video frame data copied, total size: %d bytes.", total_size);
    return out;
}

std::shared_ptr<AudioFrame> convert_audio_frame(AVStream* stream, const AVFrame* frame)
{
    auto audio_frame = std::make_shared<AudioFrame>();

    // --- 计算并设置 PTS 和 Duration ---

    // 检查输入的 AVFrame 是否有有效的时间戳
    if (frame->pts != AV_NOPTS_VALUE) {
        // 将以 time_base 为单位的整数 pts 转换为以秒为单位的 double
        // av_q2d 是一个将 AVRational (分数) 转换为 double 的辅助函数
        audio_frame->pts = frame->pts * av_q2d(stream->time_base);
    } else {
        // 如果没有 PTS，可以赋一个特殊值，或者基于之前的帧估算
        // 但对于调试，先赋一个无效值并打印警告
        audio_frame->pts = -1.0; // 表示无效
        LOGW("Audio frame found with no PTS (AV_NOPTS_VALUE).");
    }

    // 根据采样数和采样率计算这一帧的播放时长（秒）
    audio_frame->duration = (double)frame->nb_samples / frame->sample_rate;

    // --- 音频重采样 ---

    audio_frame->nb_samples = frame->nb_samples;
    audio_frame->sample_rate = frame->sample_rate;
    audio_frame->channels = frame->ch_layout.nb_channels;

    AVChannelLayout in_layout = frame->ch_layout;
    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, audio_frame->channels);

    SwrContext* swr_ctx = nullptr;
    int ret = swr_alloc_set_opts2(
        &swr_ctx,
        &out_layout, AV_SAMPLE_FMT_S16, audio_frame->sample_rate,
        &in_layout, (AVSampleFormat)frame->format, frame->sample_rate,
        0, nullptr);

    if (ret < 0 || swr_init(swr_ctx) < 0) {
        LOGE("swr_alloc_set_opts2 or swr_init failed.");
        swr_free(&swr_ctx);
        av_channel_layout_uninit(&out_layout);
        return nullptr;
    }

    // 注意：这里的 dst_nb_samples 计算方式可能会引入微小的延迟，
    // 但对于大多数情况是可接受的。
    int dst_nb_samples = av_rescale_rnd(
        swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples,
        frame->sample_rate, frame->sample_rate, AV_ROUND_UP);

    int out_buf_size = av_samples_get_buffer_size(
        nullptr, audio_frame->channels, dst_nb_samples, AV_SAMPLE_FMT_S16, 1);

    // 使用 av_mallocz 可以确保内存被清零，是一个好习惯
    uint8_t* out_buf = (uint8_t*)av_mallocz(out_buf_size);
    if (!out_buf) {
        LOGE("av_mallocz failed to allocate audio buffer.");
        swr_free(&swr_ctx);
        av_channel_layout_uninit(&out_layout);
        return nullptr;
    }

    uint8_t* out[] = { out_buf };
    int samples_converted = swr_convert(
        swr_ctx,
        out, dst_nb_samples,
        (const uint8_t**)frame->extended_data,
        frame->nb_samples);

    audio_frame->interleaved_pcm = out_buf;
    audio_frame->interleaved_size = samples_converted * audio_frame->channels * sizeof(int16_t);

    swr_free(&swr_ctx);
    av_channel_layout_uninit(&out_layout);

    return audio_frame;
}

} // namespace mp4parser
