#pragma once

#include "JpegDecoder.hpp"
#include <memory>

class ImageDisplay
{
  public:
    virtual ~ImageDisplay() = default;
    virtual bool display(const ImageData &image) = 0;
};

std::unique_ptr<ImageDisplay> createAsciiArtDisplay();