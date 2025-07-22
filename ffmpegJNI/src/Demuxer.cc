#include "Demuxer.hpp"
#include <iostream>

namespace player_utils {
// --- Demuxer Thread Function (Producer) ---
void demuxer(AVFormatContext* fmt_ctx, player_utils::PacketQueue& packet_queue, int video_stream_idx)
{
    std::cout << "[Demuxer Thread] Starting...\n";
    ffmpeg_utils::PacketRange packet_range(fmt_ctx);
    int packet_count = 0;
    for (ffmpeg_utils::Packet pkt : packet_range) {
        if (pkt.get() && pkt.streamIndex() == video_stream_idx) {
            packet_queue.push(std::move(pkt));
            packet_count++;
        } else if (pkt.get()) {
            // some others
        }
    }
    std::cout << "[Demuxer Thread] Finished reading " << packet_count << " video packets.\n";
}

}

namespace ffmpeg_utils {

PacketIterator::PacketIterator(AVFormatContext* fmt_ctx, bool end)
    : ctx_(fmt_ctx)
    , finished_(end)
{
    if (!end)
        advance();
}

void PacketIterator::advance()
{
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        finished_ = true;
        return;
    }

    int ret = av_read_frame(ctx_, pkt);
    if (ret >= 0) {
        current_ = Packet(pkt);
    } else {
        finished_ = true;
        av_packet_free(&pkt);
    }
}

PacketIterator& PacketIterator::operator++()
{
    advance();
    return *this;
}

bool PacketIterator::operator==(const PacketIterator& other) const
{
    return finished_ == other.finished_;
}

bool PacketIterator::operator!=(const PacketIterator& other) const
{
    return !(*this == other);
}
}