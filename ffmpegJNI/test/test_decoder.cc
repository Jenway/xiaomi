#include <iostream>
#include <string>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

#include "Decoder.hpp"
#include "Demuxer.hpp"
#include "MediaSource.hpp"
#include "PacketQueue.hpp"
#include "YuvFileSaver.hpp"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <media_file>\n";
        return 1;
    }

    avformat_network_init();

    std::string filename = argv[1];

    MediaSource source(filename);
    AVFormatContext* fmt_ctx = source.get_format_context();
    int video_stream_index = source.get_video_stream_index();
    AVCodecParameters* pCodecPar = source.get_video_codecpar();

    player_utils::PacketQueue packet_queue(300);

    std::unique_ptr<YuvFileSaver> p_saver;
    Decoder decoder(pCodecPar, packet_queue, [&](const AVFrame* frame) {
        if (!p_saver) {
            p_saver = std::make_unique<YuvFileSaver>("output.yuv", frame->width, frame->height);
        }
        p_saver->save_frame(frame);
    });

    std::cout << "[Main] Open video_stream_index: " << video_stream_index
              << "width: " << decoder.get_width()
              << " height :" << decoder.get_height() << "\n";

    std::thread demuxer_thread([&fmt_ctx, &packet_queue, &video_stream_index]() {
        try {
            player_utils::demuxer(fmt_ctx, packet_queue, video_stream_index);
        } catch (const std::exception& e) {
            std::cerr << "[Demuxer Thread] Exception: " << e.what() << "\n";
        }

        packet_queue.shutdown(); // Signal to consumers that no more packets will come
        std::cout << "[Demuxer Thread] Exiting.\n";
    });

    std::thread decoder_thread([&decoder]() {
        try {
            decoder.run();
        } catch (const std::exception& e) {
            std::cerr << "[Decoder Thread] Exception: " << e.what() << "\n";
        }
    });

    demuxer_thread.join();
    decoder_thread.join();

    std::cout << "[Main] All threads finished. Cleaning up FFmpeg resources.\n";
    avformat_network_deinit();

    std::cout << "[Main] Program finished successfully.\n";
    return 0;
}