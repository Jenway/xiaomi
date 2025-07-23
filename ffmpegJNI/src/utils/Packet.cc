#include "Packet.hpp"

namespace ffmpeg_utils {

Packet::Packet()
    : pkt_(av_packet_alloc())
{
}
Packet::Packet(AVPacket* pkt)
    : pkt_(pkt)
{
}
Packet::~Packet()
{
    if (pkt_ != nullptr) {
        av_packet_free(&pkt_);
    }
}

Packet::Packet(Packet&& other) noexcept
    : pkt_(other.pkt_)
{
    other.pkt_ = nullptr;
}

Packet& Packet::operator=(Packet&& other) noexcept
{
    if (this != &other) {
        if (pkt_ != nullptr) {
            av_packet_free(&pkt_);
        }
        pkt_ = other.pkt_;
        other.pkt_ = nullptr;
    }
    return *this;
}

AVPacket* Packet::get() const { return pkt_; }

int Packet::streamIndex() const
{
    if (pkt_ == nullptr) {
        return -1;
    }
    return pkt_->stream_index;
}

} // namespace ffmpeg_utils