#pragma once
#include <fstream>
#include <stdexcept>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}

class YuvFileSaver {
public:
    YuvFileSaver(const std::string& output_path, int width, int height)
        : width_(width)
        , height_(height)
    {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument("Width and height must be positive.");
        }
        output_file_.open(output_path, std::ios::binary);
        if (!output_file_.is_open()) {
            throw std::runtime_error("Failed to open output file: " + output_path);
        }
    }

    ~YuvFileSaver()
    {
        if (output_file_.is_open()) {
            output_file_.close();
        }
    }

    YuvFileSaver(const YuvFileSaver&) = delete;
    YuvFileSaver& operator=(const YuvFileSaver&) = delete;

    bool is_open() const
    {
        return output_file_.is_open();
    }

    void save_frame(const AVFrame* frame)
    {
        if (!is_open() || !frame)
            return;

        if (frame->width != width_ || frame->height != height_) {
            return;
        }

        // 写入 Y, U, V 平面，处理 padding
        // Y plane
        for (int i = 0; i < height_; ++i) {
            output_file_.write(reinterpret_cast<const char*>(frame->data[0] + i * frame->linesize[0]), width_);
        }
        // U plane
        for (int i = 0; i < height_ / 2; ++i) {
            output_file_.write(reinterpret_cast<const char*>(frame->data[1] + i * frame->linesize[1]), width_ / 2);
        }
        // V plane
        for (int i = 0; i < height_ / 2; ++i) {
            output_file_.write(reinterpret_cast<const char*>(frame->data[2] + i * frame->linesize[2]), width_ / 2);
        }
    }

private:
    int width_;
    int height_;
    std::ofstream output_file_;
};