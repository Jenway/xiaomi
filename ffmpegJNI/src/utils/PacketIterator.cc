#include "Demuxer.hpp"

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
    if (pkt == nullptr) {
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

PacketRange::PacketRange(AVFormatContext* ctx)
    : ctx_(ctx)
{
}
} // namespace ffmpeg_utils