#include "Demuxer.hpp"
#include "MediaSource.hpp"
#include "MockPacketSink.h"
#include <gtest/gtest.h>
#include <thread> // for this_thread::sleep_for

// 定义测试固件
class DemuxerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 在每个测试开始前运行
        // 1. 创建 MediaSource
        source_ = std::make_shared<MediaSource>();
        // 假设 test.mp4 在可执行文件目录下
        ASSERT_TRUE(source_->open("test.mp4")) << "Failed to open test media file.";

        // 2. 创建 Demuxer
        demuxer_ = std::make_unique<Demuxer>(source_);

        // 3. 创建 MockPacketSink
        mock_sink_ = std::make_unique<MockPacketSink>();
    }

    void TearDown() override
    {
        // 在每个测试结束后运行，确保线程被清理
        demuxer_->Stop();
    }

    std::shared_ptr<MediaSource> source_;
    std::unique_ptr<Demuxer> demuxer_;
    std::unique_ptr<MockPacketSink> mock_sink_;
};

// 测试1：Demuxer 能否成功启动、读取数据包并正常结束
TEST_F(DemuxerTest, StartsAndReceivesPacketsUntilEof)
{
    demuxer_->Start([&](Demuxer::Packet& pkt) { return (*mock_sink_)(pkt); });

    // 验证：在超时时间内，我们应该能接收到 EOF 包
    ASSERT_TRUE(mock_sink_->waitForEof(5000)) << "Did not receive EOF packet in time.";

    // 验证：接收到的数据包数量应该大于0
    EXPECT_GT(mock_sink_->getPacketCount(), 0);
}

TEST_F(DemuxerTest, PausesAndResumesCorrectly)
{
    demuxer_->Start([&](Demuxer::Packet& pkt) { return (*mock_sink_)(pkt); });

    // 1. 让它运行一会儿
    ASSERT_TRUE(mock_sink_->waitForPacketCount(10)) << "Did not receive initial packets.";

    // 2. 发出暂停指令
    demuxer_->Pause();

    // 3. 等待一小段时间，让可能“溜走”的包被处理完
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 4. 获取稳定后的数据包数量
    size_t count_when_stabilized = mock_sink_->getPacketCount();

    // 5. 再等待一小段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 6. 验证：在第二次等待后，数据包数量应该没有再增加，证明 Demuxer 确实停下来了
    ASSERT_EQ(mock_sink_->getPacketCount(), count_when_stabilized) << "Demuxer did not stabilize after pause.";

    // 7. 发出恢复指令
    demuxer_->Resume();

    // 8. 验证：数据包数量应该会继续增加
    ASSERT_TRUE(mock_sink_->waitForPacketCount(count_when_stabilized + 10)) << "Demuxer did not resume.";
} // In test/test_demuxer.cc

TEST_F(DemuxerTest, SeeksCorrectly)
{
    const double seek_target_sec = 3.0;
    demuxer_->Start([&](Demuxer::Packet& pkt) { return (*mock_sink_)(pkt); });

    demuxer_->SeekTo(seek_target_sec);

    // 1. 验证：首先应该收到一个 Flush 包。
    // 这证明 MockPacketSink 的清空逻辑被触发了。
    ASSERT_TRUE(mock_sink_->waitForFlush()) << "Did not receive a flush packet after seek.";

    // 2. 验证：在收到 Flush 包之后，应该能继续收到数据包。
    // waitForPacketCount(1) 现在是等待跳转后的第一个有效包。
    ASSERT_TRUE(mock_sink_->waitForPacketCount(1)) << "Did not receive data packet after flush.";

    // 3. 验证：收到的第一个数据包的时间戳应该在 seek 目标附近。
    // 这一步现在应该能成功了，因为 Demuxer 丢弃了所有旧包。
    int stream_idx = source_->get_video_stream_index();
    AVRational time_base = source_->get_format_context()->streams[stream_idx]->time_base;
    double first_pts_sec = mock_sink_->getFirstPacketTimestamp(time_base);

    ASSERT_GE(first_pts_sec, seek_target_sec - 1.0) << "Timestamp is too far before seek target.";
    ASSERT_LT(first_pts_sec, seek_target_sec + 1.0) << "Timestamp is too far after seek target.";
}

TEST_F(DemuxerTest, SeeksWhilePausedAndThenResumes)
{
    const double seek_target_sec = 2.0;
    demuxer_->Start([&](Demuxer::Packet& pkt) { return (*mock_sink_)(pkt); });

    ASSERT_TRUE(mock_sink_->waitForPacketCount(10));
    demuxer_->Pause();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    size_t count_when_stabilized = mock_sink_->getPacketCount();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(mock_sink_->getPacketCount(), count_when_stabilized);

    demuxer_->SeekTo(seek_target_sec);

    // 验证收到了 Flush 包。
    ASSERT_TRUE(mock_sink_->waitForFlush()) << "Did not receive flush packet while paused.";

    // =========================================================
    // FIX: 修复测试逻辑！
    // 收到 flush 后，我们期望 packet_count 变为 0。
    // =========================================================
    ASSERT_EQ(mock_sink_->getPacketCount(), 0) << "Packet count should be reset to 0 after flush.";

    // 确认没有新的数据包进来，因为 Demuxer 应该回到了暂停等待状态
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(mock_sink_->getPacketCount(), 0) << "New packets arrived while still paused after seek.";

    demuxer_->Resume();

    // 验证：现在应该能收到跳转后的新数据包了，packet_count 从 0 开始增加。
    ASSERT_TRUE(mock_sink_->waitForPacketCount(1)) << "Did not receive data after resuming from seek.";
}