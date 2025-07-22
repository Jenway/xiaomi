#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
}

#include "Demuxer.hpp"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <media_file>\n";
        return 1;
    }

    // Initialize network library (if network stream)
    avformat_network_init();

    // Open input file
    AVFormatContext* fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, argv[1], nullptr, nullptr) != 0) {
        std::cerr << "Failed to open input file\n";
        return 1;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "Failed to find stream info\n";
        avformat_close_input(&fmt_ctx);
        return 1;
    }

    try {
        ffmpeg_utils::PacketRange packet_range(fmt_ctx);

        // Use range-based for loop for clarity, relies on operator*() returning by value
        for (ffmpeg_utils::Packet pkt : packet_range) {
            // Always check if the packet is valid before using it
            if (pkt.get()) {
                std::cout << "Packet stream index: " << pkt.streamIndex() << "\n";
            } else {
                std::cerr << "Warning: Encountered a null packet in iteration.\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        // Close fmt_ctx and deinitialize network here if you return immediately.
        avformat_close_input(&fmt_ctx);
        avformat_network_deinit();
        return 1;
    }

    avformat_close_input(&fmt_ctx);
    avformat_network_deinit();

    return 0;
}