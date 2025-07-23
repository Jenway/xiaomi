#include "Demuxer.hpp"
#include "SemQueue.hpp"
#include <iostream>

using ffmpeg_utils::Packet;
using player_utils::SemQueue;

namespace player_utils {
void demuxer(AVFormatContext* fmt_ctx, SemQueue<Packet>& packet_queue, int video_stream_idx)
{
    ffmpeg_utils::PacketRange packet_range(fmt_ctx);
    int packet_count = 0;
    for (ffmpeg_utils::Packet pkt : packet_range) {
        if ((pkt.get() != nullptr) && pkt.streamIndex() == video_stream_idx) {
            // 阻塞写
            packet_queue.push(std::move(pkt));
            // 阻塞写结束
            packet_count++;
        } else if (pkt.get() != nullptr) {
            // some others
        }
    }
    std::cout << "[Demuxer Thread] Finished reading " << packet_count << " video packets.\n";
}

} // namespace player_utils
