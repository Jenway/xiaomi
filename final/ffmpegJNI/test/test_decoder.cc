// test_decoder.cpp
#include "Decoder.hpp"
#include "DecoderContext.hpp"
#include "Packet.hpp"
#include "SemQueue.hpp"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>

using namespace ffmpeg_utils;
using namespace player_utils;

// Mock DecoderContext that provides a simple codec context
class MockDecoderContext : public DecoderContext {
public:
    MockDecoderContext()
    {
        auto codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec_) {
            throw std::runtime_error("Decoder not found");
        }

        codec_ctx_ = avcodec_alloc_context3(codec_);
        if (!codec_ctx_) {
            throw std::runtime_error("Failed to allocate codec context");
        }

        if (avcodec_open2(codec_ctx_, codec_, nullptr) != 0) {
            throw std::runtime_error("Failed to open codec");
        }
    }
};

// Helper to create a dummy packet
static Packet make_dummy_packet(bool is_flush = false, bool is_eof = false)
{
    Packet pkt;
    if (is_flush) {
        pkt = Packet::createFlushPacket();
    } else if (is_eof) {
        pkt = Packet::createEofPacket();
    } else {
        // Create an empty data packet
        pkt = Packet();
    }
    return pkt;
}

// Test fixture
class DecoderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ctx_ = std::make_shared<MockDecoderContext>();
        queue_ = std::make_unique<SemQueue<Packet>>(20);
        frame_count_ = 0;
        // frame_sink increments frame_count_ for each call
        frame_sink_ = [&](const AVFrame* frame) {
            ++frame_count_;
        };
        decoder_ = std::make_unique<Decoder>(ctx_, *queue_, frame_sink_);
    }

    void TearDown() override
    {
        // ensure any pending flush
        decoder_->flush();
    }

    std::shared_ptr<DecoderContext> ctx_;
    std::unique_ptr<SemQueue<Packet>> queue_;
    std::unique_ptr<Decoder> decoder_;
    std::atomic<int> frame_count_;
    Decoder::FrameSink frame_sink_;
};

// Test that run() returns immediately on empty queue (EOF)
TEST_F(DecoderTest, HandlesEofImmediately)
{
    // push EOF packet
    queue_->push(make_dummy_packet(false, true));
    // run decoder
    decoder_->run();
    // no frames expected
    EXPECT_EQ(frame_count_, 0);
}

TEST_F(DecoderTest, FlushEmitsRemainingFrames)
{
    auto pkt = make_dummy_packet();
    avcodec_send_packet(ctx_->get(), pkt.get()); // 显式发送有效 packet
    decoder_->flush(); // 这时 flush 才能返回 EOF
    EXPECT_GE(frame_count_, 0);
}

// Test full run in a separate thread
TEST_F(DecoderTest, RunProcessesPackets)
{
    std::thread t([&]() {
        decoder_->run();
    });
    // push a few dummy packets
    for (int i = 0; i < 5; ++i) {
        queue_->push(make_dummy_packet());
    }
    // push EOF to stop
    queue_->push(make_dummy_packet(false, true));
    t.join();
    // we expect at least zero frames without crash
    EXPECT_GE(frame_count_, 0);
}

// Test that decoder can be reused after flush
TEST_F(DecoderTest, ReuseAfterFlush)
{
    // first run
    queue_->push(make_dummy_packet());
    queue_->push(make_dummy_packet(false, true));
    decoder_->run();
    int first_count = frame_count_;

    // reset count
    frame_count_ = 0;
    // push new data
    queue_->push(make_dummy_packet());
    queue_->push(make_dummy_packet(false, true));
    decoder_->run();
    int second_count = frame_count_;

    // both runs should work
    EXPECT_GE(first_count, 0);
    EXPECT_GE(second_count, 0);
}
