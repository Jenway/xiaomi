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

private:
    AVPacket* pkt_ = nullptr;
};

}