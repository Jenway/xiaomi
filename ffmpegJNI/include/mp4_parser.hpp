#ifndef MP4_PARSER_H
#define MP4_PARSER_H

#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

// AVPacket 的信息
struct PacketInfo {
    int64_t pts;
    int64_t dts;
    int streamIndex;
};

// 解码后 AVFrame 的信息
struct FrameInfo {
    int width;
    int height;
    AVPixelFormat format;
    int64_t pts;
};

class Mp4Parser {
public:
    Mp4Parser();
    ~Mp4Parser();

    bool open(const std::string& filePath);
    void close();

    int getVideoStreamIndex() const { return this->videoStreamIndex; }
    AVFormatContext* getFmtCtx() { return fmtCtx; }
    
    // 读取一个视频包，获取 pts/dts 等信息（用于作业3）
    bool readNextVideoPacket(PacketInfo& packetInfo);

    // 获取所有视频包信息（用于批量输出）
    std::vector<PacketInfo> readAllVideoPackets();

    // 解码下一个视频帧，获取帧宽高、格式等信息（用于作业4）
    bool decodeNextVideoFrame(FrameInfo& frameInfo);

    // 导出解码后的帧为 YUV420P 并保存到指定文件（用于作业5）
    bool dumpAllFramesToYUV(const std::string& yuvOutputPath);



private:
    AVFormatContext* fmtCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;
    AVCodec* decoder = nullptr;
    SwsContext* swsCtx = nullptr;

    int videoStreamIndex = -1;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* yuvFrame = nullptr;

    int dstWidth = 0;
    int dstHeight = 0;
    AVPixelFormat dstFormat = AV_PIX_FMT_YUV420P;

    bool initSwsIfNeeded();
    bool convertToYUV(AVFrame* src, AVFrame* dst);
    bool writeYUVFrame(FILE* outFile, AVFrame* frame);
};

#endif // MP4_PARSER_H
