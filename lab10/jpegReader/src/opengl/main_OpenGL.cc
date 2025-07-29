#include "JpegDecoder.hpp"
#include "OpenGLDisplay.hpp"
#include <iostream>
#include <memory>
#include <string>

#include <iostream>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <jpeg_file>" << std::endl;
        return 1;
    }

    auto decoder = createJpegDecoder();
    auto image = decoder->decode(argv[1]);
    if (!image)
    {
        std::cerr << "Failed to decode image" << std::endl;
        return 1;
    }

    auto display = createOpenGLDisplay();
    if (!display->display(image))
    {
        std::cerr << "Failed to display image" << std::endl;
        return 1;
    }

    return 0;
}
