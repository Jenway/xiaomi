#include "Mp4Parser.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

#include <android/log.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

#include "Decoder.hpp"
#include "Demuxer.hpp"
#include "MediaSource.hpp"
#include "PacketQueue.hpp"

namespace mp4parser {

struct Mp4Parser::Impl {
    Config config;
    Callbacks callbacks;

    std::atomic<PlayerState> state { PlayerState::Stopped };
    std::thread demuxer_thread;
    std::thread decoder_thread;

    std::mutex pause_mutex;
    std::condition_variable pause_cv;
    bool paused = false;
    bool stopped = false;

    std::unique_ptr<MediaSource> source;
    std::unique_ptr<player_utils::PacketQueue> packet_queue;
    std::unique_ptr<Decoder> decoder;

    Impl(Config cfg, Callbacks cbs)
        : config(std::move(cfg))
        , callbacks(std::move(cbs))
    {
    }

    void run()
    {
        try {
            source = std::make_unique<MediaSource>(config.file_path);
            packet_queue = std::make_unique<player_utils::PacketQueue>(config.max_packet_queue_size);
            decoder = std::make_unique<Decoder>(source->get_video_codecpar(), *packet_queue,
                [this](const AVFrame* frame) {
                    std::unique_lock<std::mutex> lock(pause_mutex);
                    pause_cv.wait(lock, [this] { return !paused || stopped; });
                    if (callbacks.on_frame_decoded && !stopped)
                        callbacks.on_frame_decoded(frame);
                });

            set_state(PlayerState::Running);

            demuxer_thread = std::thread(&Impl::demuxer_loop, this);
            decoder_thread = std::thread(&Impl::decoder_loop, this);

        } catch (const std::exception& e) {
            report_error(e.what());
        }
    }

    void demuxer_loop()
    {
        try {
            player_utils::demuxer(source->get_format_context(), *packet_queue, source->get_video_stream_index());
        } catch (const std::exception& e) {
            report_error(e.what());
        }
        packet_queue->shutdown();
    }

    void decoder_loop()
    {
        try {
            decoder->run();
            set_state(PlayerState::Finished);
        } catch (const std::exception& e) {
            report_error(e.what());
        }
    }

    void set_state(PlayerState s)
    {
        state = s;
        if (callbacks.on_state_changed)
            callbacks.on_state_changed(s);
    }

    void report_error(const std::string& msg)
    {
        state = PlayerState::Error;
        if (callbacks.on_error)
            callbacks.on_error(msg);
    }

    void do_pause()
    {
        paused = true;
        set_state(PlayerState::Paused);
    }

    void do_resume()
    {
        paused = false;
        pause_cv.notify_all();
        set_state(PlayerState::Running);
    }

    void do_stop()
    {
        stopped = true;
        pause_cv.notify_all();
        if (demuxer_thread.joinable())
            demuxer_thread.join();
        if (decoder_thread.joinable())
            decoder_thread.join();
        set_state(PlayerState::Stopped);
    }
};

std::unique_ptr<Mp4Parser> Mp4Parser::create(const Config& config, const Callbacks& callbacks)
{
    auto instance = std::unique_ptr<Mp4Parser>(new Mp4Parser());
    instance->impl_ = std::make_unique<Impl>(config, callbacks);
    return instance;
}

void Mp4Parser::start()
{
    if (impl_)
        impl_->run();
}

void Mp4Parser::pause()
{
    if (impl_)
        impl_->do_pause();
}

void Mp4Parser::resume()
{
    if (impl_)
        impl_->do_resume();
}

void Mp4Parser::stop()
{
    if (impl_)
        impl_->do_stop();
}

PlayerState Mp4Parser::get_state() const
{
    return impl_ ? impl_->state.load() : PlayerState::Error;
}

Mp4Parser::~Mp4Parser()
{
    stop();
}

void log_info(const std::string& msg)
{
    __android_log_print(ANDROID_LOG_INFO, "mp4parser", "%s", msg.c_str());
}

std::string describe_frame_info(const AVFrame* frame)
{
    if (!frame) {
        return "AVFrame is nullptr";
    }

    std::ostringstream oss;
    oss << "AVFrame info: ";
    oss << "width=" << frame->width << ", ";
    oss << "height=" << frame->height << ", ";
    oss << "format=" << frame->format << ", ";
    oss << "pts=" << frame->pts << ", ";

    for (int i = 0; i < 4; ++i) {
        if (frame->data[i]) {
            oss << "data[" << i << "]=" << static_cast<void*>(frame->data[i])
                << ", linesize[" << i << "]=" << frame->linesize[i] << "; ";
        }
    }

    return oss.str();
}

} // namespace mp4parser
