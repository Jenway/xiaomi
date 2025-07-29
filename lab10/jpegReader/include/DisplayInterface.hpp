#pragma once

#include "JpegDecoder.hpp"

class ImageDisplay
{
  public:
    virtual ~ImageDisplay() = default;
    virtual bool display(const ImageData &image) = 0;
};
