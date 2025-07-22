// packet.hpp
#pragma once
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace ffmpeg_utils {

class Packet {
public:
    Packet() { pkt_ = av_packet_alloc(); }
    // Make explicit and public for use by PacketIterator
    explicit Packet(AVPacket* pkt)
        : pkt_(pkt)
    {
    }
    ~Packet()
    {
        if (pkt_)
            av_packet_free(&pkt_);
    }

    // 不允许拷贝
    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;

    // 允许移动
    Packet(Packet&& other) noexcept
        : pkt_(other.pkt_)
    {
        other.pkt_ = nullptr;
    }

    Packet& operator=(Packet&& other) noexcept
    {
        if (this != &other) {
            if (pkt_)
                av_packet_free(&pkt_);
            pkt_ = other.pkt_;
            other.pkt_ = nullptr;
        }
        return *this;
    }

    AVPacket* get() const { return pkt_; }

    int streamIndex() const
    {
        if (!pkt_) {
            return -1;
        }
        return pkt_->stream_index;
    }

private:
    AVPacket* pkt_ = nullptr;
};

}