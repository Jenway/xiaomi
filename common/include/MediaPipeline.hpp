#pragma once
#include "AAudioRender.h"
#include "Entitys.hpp"
#include "GLRenderHost.hpp"
#include "Mp4Parser.hpp"
#include "SemQueue.hpp"
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <android/native_window.h>
#include <memory>

class MediaPipeline {
public:
    MediaPipeline();
    ~MediaPipeline();

    bool initialize(const mp4parser::Config& config, ANativeWindow* window, const mp4parser::Callbacks& callbacks);

    void start();
    void stop();
    void pause(bool is_paused);
    void seek(double position);
    void flush();

    [[nodiscard]] player_utils::AudioParams getAudioParams() const;
    [[nodiscard]] double getDuration() const;

    std::unique_ptr<mp4parser::Mp4Parser> parser_;
    std::unique_ptr<render_utils::GLRenderHost> video_render_;
    std::unique_ptr<AAudioRender> audio_render_;
    std::unique_ptr<player_utils::SemQueue<std::shared_ptr<player_utils::VideoFrame>>> video_frame_queue_;
    std::unique_ptr<player_utils::SemQueue<std::shared_ptr<player_utils::AudioFrame>>> audio_frame_queue_;
};