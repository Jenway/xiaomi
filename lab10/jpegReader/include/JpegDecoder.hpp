#pragma once

#include <memory>
#include <string>
#include <vector>

struct ImageData
{
    std::vector<unsigned char> pixel_data; // Raw RGB bytes
    unsigned int width = 0;
    unsigned int height = 0;
    int channels = 0; // Should be 3 for RGB

    explicit operator bool() const
    {
        return static_cast<bool>(this);
    }

    bool is_valid() const
    {
        return !pixel_data.empty() && width > 0 && height > 0 && channels > 0;
    }
};

class JpegDecoder
{
  public:
    virtual ~JpegDecoder() = default;
    virtual ImageData decode(const std::string &filepath) = 0;
};

std::unique_ptr<JpegDecoder> createJpegDecoder();