#include "MediaPipeline.hpp"
#include "AAudioRender.h"
#include "Entitys.hpp"
#include "GLRenderHost.hpp"
#include "Mp4Parser.hpp"
#include "SemQueue.hpp"
#include <android/log.h>
#include <cmath>
#include <memory>

#define LOG_TAG "MediaPipeline"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

using mp4parser::Mp4Parser;
using player_utils::AudioFrame;
using player_utils::AudioParams;
using player_utils::SemQueue;
using player_utils::VideoFrame;
using render_utils::GLRenderHost;
using std::make_unique;
using std::shared_ptr;
MediaPipeline::MediaPipeline() = default;

MediaPipeline::~MediaPipeline()
{
    LOGI("MediaPipeline destructor called. Cleaning up resources.");
    stop();
}

bool MediaPipeline::initialize(
    const mp4parser::Config& config,
    ANativeWindow* window,
    const mp4parser::Callbacks& callbacks)
{
    LOGI("Initializing MediaPipeline...");

    // ---  Queue ---
    video_frame_queue_ = make_unique<SemQueue<shared_ptr<VideoFrame>>>(30);
    audio_frame_queue_ = make_unique<SemQueue<shared_ptr<AudioFrame>>>(60);
    LOGI("Frame queues created.");

    video_render_ = GLRenderHost::create();
    if (!video_render_ || !video_render_->init(window)) {
        LOGE("GLRenderHost initialization failed.");
        stop();
        return false;
    }
    LOGI("GLRenderHost initialized.");

    // ---  Demuxer && Decoder ---
    parser_ = Mp4Parser::create(config, callbacks);
    if (!parser_) {
        LOGE("Mp4Parser creation failed.");
        stop();
        return false;
    }
    LOGI("Mp4Parser created.");

    // ---  Audio Render ---
    AudioParams audio_params = parser_->getAudioParams();
    if (audio_params.sample_rate <= 0 || audio_params.channel_count <= 0) {
        LOGE("Failed to get valid audio parameters from parser. Rate=%d, Channels=%d",
            audio_params.sample_rate, audio_params.channel_count);
        stop();
        return false;
    }
    LOGI("Audio params retrieved: Rate=%d, Channels=%d", audio_params.sample_rate, audio_params.channel_count);

    audio_render_ = make_unique<AAudioRender>();
    audio_render_->configure(audio_params.sample_rate, audio_params.channel_count, AAUDIO_FORMAT_PCM_I16);

    // Note the `Audio Data Callback` should be setting

    LOGI("AAudioRender configured.");

    LOGI("MediaPipeline initialization successful.");

    return true;
}

void MediaPipeline::start()
{
    LOGI("Starting MediaPipeline flow.");
    if (parser_) {
        parser_->start();
    }
    if (audio_render_) {
        audio_render_->start();
    }
    if (video_render_) {
        video_render_->start();
    }
}

void MediaPipeline::stop()
{
    LOGI("Stopping MediaPipeline and cleaning up resources.");

    if (parser_) {
        parser_->stop();
        parser_.reset();
    }

    if (audio_render_) {
        audio_render_.reset();
    }

    if (video_render_) {
        video_render_->release();
        video_render_.reset();
    }

    if (video_frame_queue_) {
        video_frame_queue_->shutdown();
        video_frame_queue_.reset();
    }
    if (audio_frame_queue_) {
        audio_frame_queue_->shutdown();
        audio_frame_queue_.reset();
    }
    LOGI("All pipeline resources have been released.");
}

void MediaPipeline::pause(bool is_paused)
{
    LOGI("MediaPipeline pause requested: %d", is_paused);
    if (parser_) {
        is_paused ? parser_->pause() : parser_->resume();
    }
    if (video_render_) {
        is_paused ? video_render_->pause() : video_render_->resume();
    }
    // 这里不 hard pause
    // 在 Nativeplayer::impl 中 soft pause
}

void MediaPipeline::seek(double position, std::shared_ptr<std::promise<void>> promise)
{
    LOGI("MediaPipeline seeking to %.2f seconds.", position);
    if (parser_) {
        // --- FIX: Pass the promise down to the parser ---
        parser_->seek(position, std::move(promise));
    } else {
        promise->set_value(); // Fulfill if no parser exists
    }
}
void MediaPipeline::flush()
{
    LOGI("Flushing pipeline buffers.");
    if (video_render_) {
        video_render_->flush();
    }
    if (audio_render_) {
        audio_render_->flush();
    }

    LOGI("Pipeline buffers flushed.");
}

player_utils::AudioParams MediaPipeline::getAudioParams() const
{
    if (parser_) {
        return parser_->getAudioParams();
    }
    return {};
}

double MediaPipeline::getDuration() const
{
    if (parser_) {
        return parser_->get_duration();
    }
    return NAN;
}