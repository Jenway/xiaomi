#include "mp4_parser.hpp"

#include <cstring>
#include <iostream>

Mp4Parser::Mp4Parser()
{
    avformat_network_init();
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    yuvFrame = av_frame_alloc();
}

Mp4Parser::~Mp4Parser()
{
    close();
    av_packet_free(&packet);
    av_frame_free(&frame);
    av_frame_free(&yuvFrame);
    avformat_network_deinit();
}

bool Mp4Parser::open(const std::string& filePath)
{
    if (avformat_open_input(&fmtCtx, filePath.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Failed to open input file: " << filePath << std::endl;
        return false;
    }

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        std::cerr << "Failed to get stream info" << std::endl;
        return false;
    }

    // 找到视频流
    const AVCodec* decoder = nullptr;
    videoStreamIndex = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (videoStreamIndex < 0) {
        std::cerr << "No video stream found" << std::endl;
        return false;
    }

    codecCtx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(codecCtx, fmtCtx->streams[videoStreamIndex]->codecpar);

    if (avcodec_open2(codecCtx, decoder, nullptr) < 0) {
        std::cerr << "Failed to open codec" << std::endl;
        return false;
    }

    dstWidth = codecCtx->width;
    dstHeight = codecCtx->height;
    return true;
}

bool Mp4Parser::readNextVideoPacket(PacketInfo& info)
{
    while (av_read_frame(fmtCtx, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            info.pts = packet->pts;
            info.dts = packet->dts;
            info.streamIndex = packet->stream_index;
            av_packet_unref(packet);
            return true;
        }
        av_packet_unref(packet);
    }
    return false;
}

std::vector<PacketInfo> Mp4Parser::readAllVideoPackets()
{
    std::vector<PacketInfo> packets;
    av_seek_frame(fmtCtx, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
    while (av_read_frame(fmtCtx, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            PacketInfo info { packet->pts, packet->dts, packet->stream_index };
            packets.push_back(info);
        }
        av_packet_unref(packet);
    }
    return packets;
}

bool Mp4Parser::decodeNextVideoFrame(FrameInfo& frameInfo)
{
    while (av_read_frame(fmtCtx, packet) >= 0) {
        if (packet->stream_index != videoStreamIndex) {
            av_packet_unref(packet);
            continue;
        }
        if (avcodec_send_packet(codecCtx, packet) < 0) {
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);

        if (avcodec_receive_frame(codecCtx, frame) == 0) {
            frameInfo.width = frame->width;
            frameInfo.height = frame->height;
            frameInfo.format = static_cast<AVPixelFormat>(frame->format);
            frameInfo.pts = frame->pts;
            return true;
        }
    }
    return false;
}

bool Mp4Parser::initSwsIfNeeded()
{
    if (swsCtx)
        return true;
    swsCtx = sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, dstFormat,
        SWS_BICUBIC, nullptr, nullptr, nullptr);

    if (!swsCtx) {
        std::cerr << "Failed to init swscale context" << std::endl;
        return false;
    }

    int bufSize = av_image_alloc(
        yuvFrame->data, yuvFrame->linesize,
        codecCtx->width, codecCtx->height, dstFormat, 1);
    return bufSize > 0;
}

bool Mp4Parser::convertToYUV(AVFrame* src, AVFrame* dst)
{
    if (!initSwsIfNeeded())
        return false;
    sws_scale(swsCtx,
        src->data, src->linesize, 0, src->height,
        dst->data, dst->linesize);
    return true;
}

bool Mp4Parser::writeYUVFrame(FILE* outFile, AVFrame* frame)
{
    int w = codecCtx->width;
    int h = codecCtx->height;
    fwrite(frame->data[0], 1, w * h, outFile); // Y
    fwrite(frame->data[1], 1, w * h / 4, outFile); // U
    fwrite(frame->data[2], 1, w * h / 4, outFile); // V
    return true;
}

bool Mp4Parser::dumpAllFramesToYUV(const std::string& path)
{
    FILE* outFile = fopen(path.c_str(), "wb");
    if (!outFile) {
        std::cerr << "Failed to open output YUV file" << std::endl;
        return false;
    }

    av_seek_frame(fmtCtx, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
    while (av_read_frame(fmtCtx, packet) >= 0) {
        if (packet->stream_index != videoStreamIndex) {
            av_packet_unref(packet);
            continue;
        }
        if (avcodec_send_packet(codecCtx, packet) < 0) {
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);

        while (avcodec_receive_frame(codecCtx, frame) == 0) {
            if (!convertToYUV(frame, yuvFrame)) {
                fclose(outFile);
                return false;
            }
            writeYUVFrame(outFile, yuvFrame);
        }
    }
    fclose(outFile);
    return true;
}

void Mp4Parser::close()
{
    if (codecCtx) {
        avcodec_free_context(&codecCtx);
        codecCtx = nullptr;
    }
    if (fmtCtx) {
        avformat_close_input(&fmtCtx);
        fmtCtx = nullptr;
    }
    if (swsCtx) {
        sws_freeContext(swsCtx);
        swsCtx = nullptr;
    }
}
