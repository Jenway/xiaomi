#include "SemQueue.hpp"
#include "Packet.hpp"

namespace player_utils {
void demuxer(AVFormatContext* fmt_ctx, player_utils::SemQueue<ffmpeg_utils::Packet>& packet_queue, int video_stream_idx);
}

namespace ffmpeg_utils {

class PacketIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = Packet;

    PacketIterator(AVFormatContext* fmt_ctx, bool end = false);

    PacketIterator& operator++();

    Packet operator*() { return std::move(current_); }
    bool operator==(const PacketIterator& other) const;
    bool operator!=(const PacketIterator& other) const;

private:
    AVFormatContext* ctx_;
    Packet current_;
    bool finished_;
    void advance();
};

class PacketRange {
public:
    explicit PacketRange(AVFormatContext* ctx)
        : ctx_(ctx)
    {
    }
    [[nodiscard]] PacketIterator begin() const { return { ctx_, false }; }
    [[nodiscard]] PacketIterator end() const { return { ctx_, true }; }

private:
    AVFormatContext* ctx_;
};

}
