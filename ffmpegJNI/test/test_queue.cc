#include <chrono>
#include <iostream>
#include <string>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "Decoder.hpp"
#include "Demuxer.hpp"
#include "MediaSource.hpp"
#include "PacketQueue.hpp"
#include "YuvFileSaver.hpp"

// --- Demuxer Thread Function (Producer) ---
void demuxer_thread_func(AVFormatContext* fmt_ctx, player_utils::PacketQueue& packet_queue)
{
    std::cout << "[Demuxer Thread] Starting...\n";
    try {
        ffmpeg_utils::PacketRange packet_range(fmt_ctx);
        int packet_count = 0;
        for (ffmpeg_utils::Packet pkt : packet_range) {
            if (pkt.get()) {
                std::cout << "[Demuxer Thread] Read packet (stream: " << pkt.streamIndex() << ") - Pushing to queue...\n";
                packet_queue.push(std::move(pkt));
                packet_count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                std::cerr << "[Demuxer Thread] Warning: Read a null packet.\n";
            }
        }
        std::cout << "[Demuxer Thread] Finished reading " << packet_count << " packets.\n";
    } catch (const std::exception& e) {
        std::cerr << "[Demuxer Thread] Exception: " << e.what() << "\n";
    }

    packet_queue.shutdown();
    std::cout << "[Demuxer Thread] Exiting.\n";
}

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

    player_utils::PacketQueue packet_queue(300);

    std::unique_ptr<YuvFileSaver> p_saver;
    auto decoder_context = std::make_shared<DecoderContext>(source.get_video_codecpar());

    Decoder decoder(decoder_context, packet_queue, [&](const AVFrame* frame) {
        std::cout << frame->width << " * " << frame->height << "\n";
    });

    std::thread demuxer_thread(demuxer_thread_func, fmt_ctx, std::ref(packet_queue));
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