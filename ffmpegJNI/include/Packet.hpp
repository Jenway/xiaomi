// packet.hpp
#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace ffmpeg_utils {

class Packet {
public:
    Packet();
    explicit Packet(AVPacket* pkt);
    ~Packet();
    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;
    Packet(Packet&& other) noexcept;
    Packet& operator=(Packet&& other) noexcept;

    [[nodiscard]] AVPacket* get() const;
    [[nodiscard]] int streamIndex() const;

    static Packet createFlushPacket()
    {
        Packet pkt;
        pkt.is_flush_packet_ = true;
        av_packet_free(&pkt.pkt_);
        pkt.pkt_ = nullptr;
        return pkt;
    }

    static Packet createEofPacket()
    {
        Packet pkt;
        pkt.is_eof_packet_ = true;
        av_packet_free(&pkt.pkt_);
        pkt.pkt_ = nullptr;
        return pkt;
    }

    // 添加查询方法
    bool isFlush() const { return is_flush_packet_; }
    bool isEof() const { return is_eof_packet_; }
    bool isData() const { return pkt_ != nullptr && !is_flush_packet_ && !is_eof_packet_; }

private:
    AVPacket* pkt_ = nullptr;
    bool is_flush_packet_ = false;
    bool is_eof_packet_ = false;
};

} // namespace ffmpeg_utils
